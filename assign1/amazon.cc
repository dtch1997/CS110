#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <algorithm>
#include "amazon.h"
#include <iostream>
#include <tuple>
#include <set>
#include <unordered_map>
#include <assert.h>

using namespace std;

vector<vector<string>> convertQuery(string query);

amazon::amazon(const string& directory, const string& filesPrefix) {
    const string databaseFileName = directory + "/" + filesPrefix + ".bin";
    const string keywordIndexFileName = directory + "/" + filesPrefix + "_keyword_index.bin";  
    databaseFile = acquireFileMap(databaseFileName, databaseInfo);
    keywordIndexFile = acquireFileMap(keywordIndexFileName, keywordIndexInfo);
}


bool amazon::good() const {
    return !( (databaseInfo.fd == -1) || 
            (keywordIndexInfo.fd == -1) ); 
}

amazon::~amazon() {
    releaseFileMap(databaseInfo);
    releaseFileMap(keywordIndexInfo);
}

const char * amazon::findKeywordPtr(const std::string& keyword) const {
    unsigned int * offsetArrayStart = (unsigned int *) keywordIndexFile + 1;
    unsigned int numKeywords = * (unsigned int *) keywordIndexFile;    
   
    // Find the keyword in the keywordIndexFile
    auto keywordIndexFile = this->keywordIndexFile;
    auto compare = [keywordIndexFile](unsigned int offset, std::string referenceKeyword) -> bool {
        const char * str = (char *) keywordIndexFile + offset;
        std::string queryKeyword = std::string(str);
        return queryKeyword < referenceKeyword;
    };
    auto offsetArrayPtr = std::lower_bound(offsetArrayStart, offsetArrayStart + numKeywords, keyword, compare);
    // If keyword not found, return a nullptr
    if (offsetArrayPtr == offsetArrayStart + numKeywords) return nullptr; 
    auto offset = * offsetArrayPtr;
    const char * queryKeyword = (char *) keywordIndexFile + offset; 
    // If keyword not found, return a nullptr
    if (std::string(queryKeyword) != keyword) return nullptr;
    return queryKeyword;
}

struct KeywordInfo {
    unsigned int reviewIndex;
    unsigned char reviewPortion;
    unsigned int wordOffset;

    // We need to overload the comparison operator to allow insertion into a set
    bool operator<(const KeywordInfo& other) const {
        return reviewIndex < other.reviewIndex ||
            (reviewIndex == other.reviewIndex && reviewPortion < other.reviewPortion) ||
            (reviewIndex == other.reviewIndex && reviewPortion == other.reviewPortion && wordOffset < other.wordOffset);
    }
};

KeywordInfo parse_keyword_info(const unsigned int * const keywordInfoBlock) {
    KeywordInfo keywordInfo;
    keywordInfo.reviewIndex = *keywordInfoBlock;
    unsigned int keywordInfoByte = *(keywordInfoBlock + 1);
    keywordInfo.reviewPortion = (keywordInfoByte & 0xFF000000) >> 24;
    keywordInfo.wordOffset = (keywordInfoByte & 0x00FFFFFF);
    return keywordInfo;
}

std::set<unsigned int> amazon::findReviewsContainingTerm(const std::vector<std::string>& term) const {
    std::set<unsigned int> reviewIndices;
    std::set<KeywordInfo> keywordInfoSet;
    std::set<KeywordInfo> newKeywordInfoSet;
    bool isFirst = true; // Is it the first iteration of the outer loop?

    for (auto it = term.begin(); it != term.end(); ++it)  {
        newKeywordInfoSet.clear();
        const char * keywordPtr = findKeywordPtr(*it);
        if (keywordPtr == nullptr) {
            // Keyword was not found in the database, i.e no matching reviews
            // Return empty set
            reviewIndices.clear();
            return reviewIndices;
        }

        const std::string keyword = keywordPtr;
        assert(keyword == *it);
        // Pointer arithmetic to find start of array containing review info w.r.t keyword
        // If keyword has even length, null terminator makes it odd length, so add one more byte to make it even
        const size_t keywordBytes = keyword.length() % 2 == 0 ? keyword.length() + 2 : keyword.length() + 1; 
        
        const unsigned int numEntries = *(unsigned int *)(keywordPtr + keywordBytes);
        const unsigned int * keywordInfoBlockStart = (unsigned int *)(keywordPtr + keywordBytes + 4);
        
        // Iterate over all reviews in the keyword information block
        for (unsigned int i = 0; i < numEntries; i ++) {
            KeywordInfo keywordInfo = parse_keyword_info(keywordInfoBlockStart + 2*i);
            if (isFirst) {
                // Add every review containing first keyword to the set
                newKeywordInfoSet.insert(keywordInfo);
            } 
            else {
                // Only add reviews containing previous keyword in correct position
                // Because we iterate from first keyword to last, we need only cache the previous keyword's location, not its value
                KeywordInfo prevKeywordInfo = keywordInfo;
                prevKeywordInfo.wordOffset -= 1;
                const bool prev_is_in = keywordInfoSet.find(prevKeywordInfo) != keywordInfoSet.end();
                if (prev_is_in) newKeywordInfoSet.insert(keywordInfo);
            }
        }
        keywordInfoSet = newKeywordInfoSet;

        // After the first outer loop, behaviour changes
        isFirst = false;
     
    }
    // keywordInfoSet now contains the review indices that match the entire term
    for (const auto & element : keywordInfoSet) {
        reviewIndices.insert(element.reviewIndex);
    }

    return reviewIndices;
}


bool amazon::searchKeywordIndex(const string& query, vector<unsigned int>& reviewIndexes) const {
    vector<vector<string>> allSearchTerms = convertQuery(query);
    std::set<unsigned int> reviewIndices;
    bool isFirst = true;
    for (auto& term : allSearchTerms) {
         std::set<unsigned int> currentTermReviewIndices = findReviewsContainingTerm(term);
         if (isFirst) {
             isFirst = false;
             reviewIndices = currentTermReviewIndices;
         }
         else {
             std::set<unsigned int> intersect;

             std::set_intersection(
                 currentTermReviewIndices.begin(), currentTermReviewIndices.end(),
                 reviewIndices.begin(), reviewIndices.end(),
                 std::inserter(intersect, intersect.begin())
             );
             reviewIndices = intersect;

         }
    }

    for (unsigned int element: reviewIndices) {
        reviewIndexes.push_back(element);
    }
    return reviewIndexes.size() > 0 ;
}

const char * getElementStartPtr(const void * const file, const unsigned int index) {
    // Helper function for processing the array of offsets at the start of databaseFile and keywordIndexFile
    const unsigned int * offsetArrayStart = (unsigned int *) file + 1;
    unsigned int offset = offsetArrayStart[index];
    char * elementStartPtr = (char*) file + offset;
    return elementStartPtr;
}

const char * convert_and_advance(const char * const str, std::string& string) {
    // Convert a C string to a C++ string
    // Return a pointer to the byte after the null delimiter of the C string
    string = std::string(str);
    return str + string.length() + 1;    
}

bool amazon::getReview(unsigned int index, Review &review) const {

    if (index >= totalReviews()) return false;

    const char * nextPtr = getElementStartPtr(databaseFile, index);

    review.index = index; 
    nextPtr = convert_and_advance(nextPtr, review.product_title);
    nextPtr = convert_and_advance(nextPtr, review.product_category);
    review.star_rating = *nextPtr;
    nextPtr += 1;
    nextPtr = convert_and_advance(nextPtr, review.review_headline);
    nextPtr = convert_and_advance(nextPtr, review.review_body);

    if ((review.product_title.length() + 
        review.product_category.length() + 
        review.review_headline.length() + 
        review.review_body.length() + 1) % 2 == 1) {
        nextPtr += 1;
    } 

    review.review_year = *(short *) nextPtr;
    nextPtr += 2;
    review.review_month = *nextPtr;
    nextPtr += 1;
    review.review_day = *nextPtr;
   
    return true;
}

void amazon::getSortedReviewsFromIndexes(const vector<unsigned int> &reviewIndexes,
    vector<Review> &reviews,
    function<bool(const Review &, const Review &)> cmp) const {

    for (auto index: reviewIndexes) {
        Review review;
        getReview(index, review);
        reviews.push_back(review);
    }
    std::sort(reviews.begin(), reviews.end(), cmp);
}

// Other functions below (no need to change)
ostream& operator<<(ostream& os, const Review& review) {
    os << "Review index: " << review.index << endl;
    os << "Product title: " << review.product_title << endl;
    os << "Product category: " << review.product_category << endl;
    os << "Star rating: " << review.star_rating << " stars" << endl;
    os << "Review headline: " << review.review_headline << endl;
    os << "Review body: " << review.review_body << endl;
    os << "Date: " << review.review_year << "-" <<
        review.review_month << "-" << 
        review.review_day << endl;
    return os;
}


const void *amazon::acquireFileMap(const string& fileName, struct fileInfo& info) {
    struct stat stats;
    stat(fileName.c_str(), &stats);
    info.fileSize = stats.st_size;
    info.fd = open(fileName.c_str(), O_RDONLY);
    return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void amazon::releaseFileMap(struct fileInfo& info) {
    if (info.fileMap != NULL) munmap((char *) info.fileMap, info.fileSize);
    if (info.fd != -1) close(info.fd);
}

vector<vector<string>> convertQuery(string query) {
    /* Converts to lowercase, removes punctuation, and then returns a vector of the search terms,
       as a vector of string vectors. Each search term will have its individual words
       as a separate vector. Words in quotes will be a single vector, as will hyphenated words.
       E.g.,
       TV "did not work" second-rate
       will become:
       [['tv'], ['did', 'not', 'work'], ['second', 'rate']]
       */

    // make lowercase, convert dashes to space, and remove characters 
    // that aren't alphanumeric, or the double-quote, or a space
    string newStr = "";
    for (unsigned char c : query) {
        c = tolower(c);
        if (isalnum(c) || c == '"' || c == ' ' || c == '-') {
            newStr += c;
        }
    }
    query = newStr;

    vector<vector<string>> allSearchTerms;
    vector<string> currentTerms;
    string currentTerm;
    bool quoteFound = false;

    for (unsigned char c : query) {
        if (c == '"') {
            quoteFound = !quoteFound;
            if (!quoteFound) { // just got to end of the phrase
                if (currentTerm != "") {
                    currentTerms.push_back(currentTerm);
                    currentTerm = "";
                }
            }
        } else if (c == ' ') {
            if (currentTerm != "") {
                currentTerms.push_back(currentTerm);
                currentTerm = "";
            }
            if (!quoteFound) {
                if (currentTerms.size() != 0) {
                    allSearchTerms.push_back(currentTerms);
                    currentTerms.clear();
                }
            }
        } else if (c == '-') {
            if (currentTerm != "") {
                currentTerms.push_back(currentTerm);
                currentTerm = "";
            }
        } else {
            currentTerm += c;
        }
    }

    if (currentTerm != "") {
        currentTerms.push_back(currentTerm);
    }
    if (currentTerms.size() != 0) {
        allSearchTerms.push_back(currentTerms);
    }
    return allSearchTerms;
}

