/**
 * File: log.h
 * -----------
 * Exports a class that's dedicated to printing out structured info, warning, and error messages.
 */

#pragma once
#include <string>
#include "article.h"

class NewsAggregatorLog {
 public:
  NewsAggregatorLog(bool verbose): verbose(verbose) {} 
  
  // Prints out the message and executable with a message on available usage flags.
  static void printUsage(const std::string& message, const std::string& executableName);
  
  // Log for when we failed to parse a feed list
  void noteFullRSSFeedListDownloadFailureAndExit(const std::string& rssFeedListURI) const;

  // Log for when we have successfully parsed a feed list
  void noteFullRSSFeedListDownloadEnd() const;

  // Log for when we have finished scheduling all feeds for this feed list to download
  void noteAllFeedsHaveBeenScheduledForFeedList(const std::string& rssFeedListfURI) const;
  
  // Log for when we are about to parse a feed
  void noteSingleFeedDownloadBeginning(const std::string& feedURI) const;

  // Log for when we skip processing a feed because we have already processed its URL
  void noteSingleFeedDownloadSkipped(const std::string& feedURI) const;

  // Log for when we failed to parse a feed
  void noteSingleFeedDownloadFailure(const std::string& feedURI) const;

  // Log for when we have finished scheduling all articles for this feed to download
  void noteAllArticlesHaveBeenScheduledForFeed(const std::string& feedURI) const;

  // Log for when we have finished downloading all feeds, including all articles
  void noteAllRSSFeedsDownloadEnd() const;
  
  // Log for when we are about to parse an article
  void noteSingleArticleDownloadBeginning(const Article& article) const;

  // Log for when we skip processing an article because we have already processed its URL
  void noteSingleArticleDownloadSkipped(const Article& article) const;

  // Log for when we failed to parse an article
  void noteSingleArticleDownloadFailure(const Article& article) const;
  
 private:
  bool verbose;
  NewsAggregatorLog(const NewsAggregatorLog& original) = delete;
  NewsAggregatorLog& operator=(const NewsAggregatorLog& rhs) = delete;
};
