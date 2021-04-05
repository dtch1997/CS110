#include <iostream>
#include <iostream>
#include <iomanip> // for setw formatter
#include <map>
#include <set>
#include <string>
#include "amazon.h"
using namespace std;

const string kAmazonDataDirectory("/usr/class/archive/cs/cs110/cs110.1204/samples/assign1");
const string kFilesPrefix("amazon_reviews_us_Electronics_v1_00");
static const int kDatabaseNotFound = 2;

enum {DATE, BODY_SIZE, STARS, TITLE_SIZE};

bool genericReviewCompare(const Review &lhs, const Review &rhs, int primaryKey, bool reversed=false) {
    size_t lhs_body_size = lhs.review_body.size();
    size_t rhs_body_size = rhs.review_body.size();
    
    size_t lhs_headline_size = lhs.review_headline.size();
    size_t rhs_headline_size = rhs.review_headline.size();
   
    size_t lhs_title_size = lhs.product_title.size();
    size_t rhs_title_size = rhs.product_title.size();

    if (primaryKey == DATE) {
        auto tLhs = tie(lhs.review_year, lhs.review_month, lhs.review_day, lhs_body_size, 
                    lhs_headline_size, lhs.star_rating, lhs_title_size);
        auto tRhs = tie(rhs.review_year, rhs.review_month, rhs.review_day, rhs_body_size, 
                    rhs_headline_size, rhs.star_rating, rhs_title_size);
        if (reversed) {
            return tRhs < tLhs;
        } else {
            return tLhs < tRhs;
        }
    } else if (primaryKey == BODY_SIZE) {
        auto tLhs = tie(lhs_body_size, lhs.review_year, lhs.review_month, lhs.review_day,
                    lhs_headline_size, lhs.star_rating, lhs_title_size);
        auto tRhs = tie(rhs_body_size, rhs.review_year, rhs.review_month, rhs.review_day,  
                    rhs_headline_size, rhs.star_rating, rhs_title_size);
        if (reversed) {
            return tRhs < tLhs;
        } else {
            return tLhs < tRhs;
        }
    } else if (primaryKey == STARS) {
        auto tLhs = tie(lhs.star_rating, lhs.product_title, lhs.review_year, lhs.review_month, lhs.review_day, lhs_body_size, 
                    lhs_headline_size);
        auto tRhs = tie(rhs.star_rating, rhs.product_title, rhs.review_year, rhs.review_month, rhs.review_day, rhs_body_size, 
                    rhs_headline_size);
        if (reversed) {
            return tRhs < tLhs;
        } else {
            return tLhs < tRhs;
        }
    } else if (primaryKey == TITLE_SIZE) {
        auto tLhs = tie(lhs_title_size, lhs.review_year, lhs.review_month, lhs.review_day, lhs_body_size, 
                    lhs_headline_size, lhs.star_rating);
        auto tRhs = tie(rhs_title_size, rhs.review_year, rhs.review_month, rhs.review_day, rhs_body_size, 
                    rhs_headline_size, rhs.star_rating);
        if (reversed) {
            return tRhs < tLhs;
        } else {
            return tLhs < tRhs;
        }
    } else {
        return false;
    }
}

static void showUsage(string name)
{
    cout << "Usage: " << name << " <option(s)> 'search string'" << endl
        << "Options:\n" << endl
        << "\t-h,--help\t\tShow this help message" << endl
        << "\t-k,--primary-key\tPrimary key, one of: date, stars, bodysize, titlesize (default is date)" << endl
        << "\t-r,--reversed\tReverse ordering for primary key, making it descending instead of ascending" << endl
        << "\t-n,--number-of-reviews\tNumber of reviews to show (default is to show all reviews)" << endl
        << "\t-d,--directory DIRECTORY\tSpecify the directory for the database files" << endl
        << "\t-f,--files-prefix FILE_PREFIX\tSpecify the files prefix (default is 'amazon_reviews_us_Electronics_v1_00')" << endl;
}

static int parseArgs(int argc, char **argv, bool &interactive, string &amazonDataDirectory, 
        string &filesPrefix, int &primaryKey, bool &reversed, size_t &numReviews, string &searchString) {
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if ((arg == "-h") || (arg == "--help")) {
            showUsage(argv[0]);
            return -1;
        } else if ((arg == "-k") || (arg == "--primary-key")) {
            if (i + 1 < argc) { // Make sure we aren't at the end of argv!
                string pKeyStr = argv[++i];
                if (pKeyStr == "date") {
                    primaryKey = DATE;
                } else if (pKeyStr == "stars") {
                    primaryKey = STARS;
                } else if (pKeyStr == "bodysize") {
                    primaryKey = BODY_SIZE;
                } else if (pKeyStr == "titlesize") {
                    primaryKey = TITLE_SIZE;
                } else {
                    cout << "--primary-key must be either date, stars, bodysize, or titlesize" << endl;
                    showUsage(argv[0]);
                    return -1;
                }
            }
        } else if ((arg == "-n") || (arg == "--number-of-reviews")) {
            if (i + 1 < argc) { // Make sure we aren't at the end of argv!
               numReviews = stoi(argv[++i]);
            } else {
                cout << "--number-of-reviews needs one argument" << endl;
                showUsage(argv[0]);
                return -1;
            }

        } else if ((arg == "-r") || (arg == "--reversed")) {
            reversed = true;
        } else if ((arg == "-d") || (arg == "--directory")) {
            if (i + 1 < argc) { // Make sure we aren't at the end of argv!
                amazonDataDirectory = argv[++i]; // Increment 'i' so we don't get the argument as the next argv[i].
            } else { // Uh-oh, there was no argument to the destination option.
                cout << "--directory option requires one argument." << endl;
                showUsage(argv[0]);
                return -1;
            }  
        } else if ((arg == "-f") || (arg == "--files-prefix")) {
            if (i + 1 < argc) { // Make sure we aren't at the end of argv!
                filesPrefix = argv[++i]; // Increment 'i' so we don't get the argument as the next argv[i].
            } else { // Uh-oh, there was no argument to the destination option.
                cout << "--files-prefix option requires one argument." << endl;
                showUsage(argv[0]);
                return -1;
            }  
        } else {
            searchString = argv[i];
        }
    }
    if (!interactive && searchString == "") {
        cout << "No search string found. Going into interactive mode" << endl;
        interactive = true;
    }
    return 0;
}

int main(int argc, char **argv) {
    bool interactive = false;
    string amazonDataDirectory = kAmazonDataDirectory;
    string filesPrefix = kFilesPrefix;
    int primaryKey = DATE;
    bool reversed = false;
    string searchString;
    size_t origNumReviews = (size_t)-1;

    if (parseArgs(argc, argv, interactive, amazonDataDirectory, filesPrefix, primaryKey, 
                reversed, origNumReviews, searchString) == -1) return -1;

    amazon db(amazonDataDirectory, filesPrefix);
    if (!db.good()) {
        cerr << "Problem reading data files...aborting!" << endl; 
        return kDatabaseNotFound;
    }

    cout << "Total number of keywords: " << db.totalKeywords() << endl;

    while (true) {
        size_t numReviews = origNumReviews;
        if (interactive) {
            cout << "Please enter a search query (<enter> to end): " << flush;
            getline(cin, searchString);
            if (searchString == "") break;
        }
        vector<unsigned int> reviewIndexes;
        if (!db.searchKeywordIndex(searchString, reviewIndexes)) {
            cout << "Could not find any matches for query '" << searchString << "'" << endl;
        } else {
            vector<Review> reviews;
            db.getSortedReviewsFromIndexes(reviewIndexes, reviews, [primaryKey, reversed](const Review &lhs, const Review &rhs) {
                    return genericReviewCompare(lhs, rhs, primaryKey, reversed);
                    });

            cout << "Found " << reviewIndexes.size() << " matching reviews out of " <<
                db.totalReviews() << " reviews in the database." << endl;
            if (interactive) {
                cout << "Press <enter> to see the first five reviews." << flush;
                string userInput;
                getline(cin, userInput);
            }

            if (numReviews == (size_t)-1 || numReviews > reviews.size()) {
                numReviews = reviews.size();
            }

            for (size_t i=0; i < numReviews; i++) {
                Review review = reviews[i];
                cout << "**********" << endl;
                cout << review << endl;
                cout << "**********" << endl << endl;

                if (interactive) {
                    if ((i + 1) % 5 == 0) {
                        cout << "Press <enter> to see the next five reviews ('q' to quit). " << flush;
                        string userInput;
                        getline(cin, userInput);
                        if (userInput != "" && tolower(userInput[0]) == 'q') break;
                    }
                }
            }
        }
        if (!interactive) break;
    }
    return 0;
}
