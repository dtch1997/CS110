/**
 * File: news-aggregator.cc
 * --------------------------------
 * Presents the implementation of the NewsAggregator class.
 */

#include "news-aggregator.h"
#include <iostream>
#include <iomanip>
#include <memory>
#include <thread>
#include <iostream>
#include <algorithm>
#include <thread>
#include <utility>

#include <getopt.h>
#include <libxml/parser.h>
#include <libxml/catalog.h>
#include "rss-feed.h"
#include "rss-feed-list.h"
#include "html-document.h"
#include "html-document-exception.h"
#include "rss-feed-exception.h"
#include "rss-feed-list-exception.h"
#include "utils.h"
#include "ostreamlock.h"
#include "string-utils.h"
using namespace std;

/**
 * Factory Method: createNewsAggregator
 * ------------------------------------
 * Factory method that spends most of its energy parsing the argument vector
 * to decide what RSS feed list to process and whether to print lots of
 * of logging information as it does so.
 */
static const string kDefaultRSSFeedListURL = "small-feed.xml";
NewsAggregator *NewsAggregator::createNewsAggregator(int argc, char *argv[]) {
  struct option options[] = {
    {"verbose", no_argument, NULL, 'v'},
    {"quiet", no_argument, NULL, 'q'},
    {"url", required_argument, NULL, 'u'},
    {NULL, 0, NULL, 0},
  };
  
  string rssFeedListURI = kDefaultRSSFeedListURL;
  bool verbose = true;
  while (true) {
    int ch = getopt_long(argc, argv, "vqu:", options, NULL);
    if (ch == -1) break;
    switch (ch) {
    case 'v':
      verbose = true;
      break;
    case 'q':
      verbose = false;
      break;
    case 'u':
      rssFeedListURI = optarg;
      break;
    default:
      NewsAggregatorLog::printUsage("Unrecognized flag.", argv[0]);
    }
  }
  
  argc -= optind;
  if (argc > 0) NewsAggregatorLog::printUsage("Too many arguments.", argv[0]);
  return new NewsAggregator(rssFeedListURI, verbose);
}

/**
 * Method: buildIndex
 * ------------------
 * Initalizex the XML parser, processes all feeds, and then
 * cleans up the parser.  The lion's share of the work is passed
 * on to processAllFeeds, which you will need to implement.
 */
void NewsAggregator::buildIndex() {
  if (built) return;
  built = true; // optimistically assume it'll all work out
  xmlInitParser();
  xmlInitializeCatalog();
  processAllFeeds();
  xmlCatalogCleanup();
  xmlCleanupParser();
}

/**
 * Method: queryIndex
 * ------------------
 * Interacts with the user via a custom command line, allowing
 * the user to surface all of the news articles that contains a particular
 * search term.
 */
void NewsAggregator::queryIndex() const {
  static const size_t kMaxMatchesToShow = 15;
  while (true) {
    cout << "Enter a search term [or just hit <enter> to quit]: ";
    string response;
    getline(cin, response);
    response = trim(response);
    if (response.empty()) break;
    const vector<pair<Article, int> >& matches = index.getMatchingArticles(response);
    if (matches.empty()) {
      cout << "Ah, we didn't find the term \"" << response << "\". Try again." << endl;
    } else {
      cout << "That term appears in " << matches.size() << " article"
           << (matches.size() == 1 ? "" : "s") << ".  ";
      if (matches.size() > kMaxMatchesToShow)
        cout << "Here are the top " << kMaxMatchesToShow << " of them:" << endl;
      else if (matches.size() > 1)
        cout << "Here they are:" << endl;
      else
        cout << "Here it is:" << endl;
      size_t count = 0;
      for (const pair<Article, int>& match: matches) {
        if (count == kMaxMatchesToShow) break;
        count++;
        string title = match.first.title;
        if (shouldTruncate(title)) title = truncate(title);
        string url = match.first.url;
        if (shouldTruncate(url)) url = truncate(url);
        string times = match.second == 1 ? "time" : "times";
        cout << "  " << setw(2) << setfill(' ') << count << ".) "
             << "\"" << title << "\" [appears " << match.second << " " << times << "]." << endl;
        cout << "       \"" << url << "\"" << endl;
      }
    }
  }
}

/**
 * Private Constructor: NewsAggregator
 * -----------------------------------
 * Self-explanatory.
 */
static const size_t kNumFeedWorkers = 8;
static const size_t kNumArticleWorkers = 64;
NewsAggregator::NewsAggregator(const string& rssFeedListURI, bool verbose): 
  log(verbose), rssFeedListURI(rssFeedListURI), built(false), feedPool(kNumFeedWorkers), articlePool(kNumArticleWorkers) {}

/**
 * Function: containsArticleUrl
 * -----------------------------------
 * Self-explanatory. 
 * No need to mutex this because std::set lookup is thread-safe. 
 * Reference: https://www.cplusplus.com/reference/set/set/insert/
 */
bool NewsAggregator::containsArticleUrl(const Article& article) const {
    std::unique_lock<std::mutex> lock(articleUrlsMutex);
    return articleUrls.find(article.url) != articleUrls.end();
}

/**
 * Function: downloadArticle
 * -----------------------------------
 * Download an article over a network connection
 * Tokenizes the HTML and saves into a Tokens& by reference
 */
void NewsAggregator::downloadArticle(const Article& article, Tokens& tokens) {
    HTMLDocument document(article.url);
    try {
        document.parse();
    } catch (const HTMLDocumentException& hde) {
        log.noteSingleArticleDownloadFailure(article);
        return;
    }
    tokens = document.getTokens();
}

/**
 * Function: updateArticle
 * -----------------------------------
 * Atomically store an article and its token vector in this->articles
 */
void NewsAggregator::updateArticle(const Article& article, const Tokens& tokens) {
    const server& server = getURLServer(article.url);
    const title& title = article.title;
    
    articlesMutex.lock();
    const auto& it1 = articles.find(server); 
    if (it1 == articles.end()) {
        articles[server][title] = std::make_pair(article, tokens);
        articlesMutex.unlock();
        return;
    }
    const auto& serverArticles = it1->second;
    const auto& it2 = serverArticles.find(article.title);
    if (it2 == serverArticles.end()) {
        articles[server][title] = std::make_pair(article, tokens);
        articlesMutex.unlock();
        return;
    }
    const auto& currentArticle = it2->second.first;
    const auto& currentTokens = it2->second.second;
    articlesMutex.unlock();


    // Perform set intersection
    Tokens t_intersection;
    std::set_intersection(
        currentTokens.begin(), currentTokens.end(),
        tokens.begin(), tokens.end(),
        std::back_inserter(t_intersection)
    );
    const Article& newArticle = (article < currentArticle) ? article : currentArticle;
    
    // Update the record 
    articlesMutex.lock();
    articles[server][title] = std::make_pair(newArticle, t_intersection);
    articlesMutex.unlock();
}

/**
 * Private Method: processArticle
 * -------------------------------
 * Process a single article with error checking
 */
void NewsAggregator::processArticle(const Article& article) {
    if (containsArticleUrl(article)) {
        log.noteSingleArticleDownloadSkipped(article);
        return;
    }
    log.noteSingleArticleDownloadBeginning(article);
    articleUrlsMutex.lock();
    articleUrls.insert(article.url);
    articleUrlsMutex.unlock();

    Tokens tokens;
    downloadArticle(article, tokens);
    std::sort(tokens.begin(), tokens.end());
    updateArticle(article, tokens);
}

/**
 * Private Method: processFeed
 * -------------------------------
 * Process a single feed with error checking. 
 */
void NewsAggregator::processFeed(const url& feedUrl, const title&  feedTitle) {
    log.noteSingleFeedDownloadBeginning(feedUrl);
    RSSFeed feed(feedUrl);
    try {
        feed.parse();
    } catch (const RSSFeedException& rfe) {
        log.noteSingleFeedDownloadFailure(feedUrl);
        return;
    }

    const vector<Article>& articles = feed.getArticles();
    semaphore completed(1-articles.size());
    for (const Article& article : articles) {
        articlePool.schedule([this, &article, &completed]() {
            processArticle(article);
            completed.signal();
        });
    }
    completed.wait();
}



/**
 * Private Method: processAllFeeds
 * -------------------------------
 */
void NewsAggregator::processAllFeeds() {
    RSSFeedList feedList(rssFeedListURI);
    try {
        feedList.parse();
    } catch (const RSSFeedListException& rfle) {
        log.noteFullRSSFeedListDownloadFailureAndExit(rssFeedListURI);
    }

    const map<string, string>& feeds = feedList.getFeeds();
    if (feeds.empty()) {
        cout << "Feed list is technically well-formed, but it's empty!" << endl;
        return;
    }

    semaphore completed(1 - feeds.size());
    for (const auto& pair : feeds ) {
        const url& feedUrl = pair.first;
        const title& feedTitle = pair.second;
        feedPool.schedule([this, &feedUrl, &feedTitle, &completed]() {
            processFeed(feedUrl, feedTitle);
            completed.signal();
        });
    }
    completed.wait();

    // Once we've processed all feeds, we can put the finalized token sets into index
    for (const auto& p : articles) {
        for (const auto& q : p.second) {
            const Article& article = q.second.first;
            const Tokens& tokens = q.second.second;
            index.add(article, tokens);
        }
    }
}
