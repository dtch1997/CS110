#pragma once
#include <string>
#include <vector>
#include <ostream>
#include <set>
#include <tuple>
#include <functional>

struct Review {
    unsigned int index;
    std::string product_title;
    std::string product_category;
    int star_rating;
    std::string review_headline;
    std::string review_body;
    int review_year;
    int review_month;
    int review_day;

    /** 
     * operator<< overload
     * ------------------
     *
     * friend function to overload the output stream operator
     * for Review, in order to allow printing via cout (for example).
     *
     * @param os The output stream to send the output to.
     * @param review The Review to send to the stream.
     */
    friend std::ostream& operator<<(std::ostream& os, const Review& review);
};

class amazon {
    public:

        /**
         * Constructor: amazon
         * -------------------
         * Constructs an amazon instance to layer on top of raw memory representations
         * stored in the specified directory.  The understanding is that the specified
         * directory contains binary files carefully formatted to compactly store
         * all of the information about the reviews, and to also store a keyword
         * index to those reviews.
         *
         * @param directory The name of the directory housing the formatted information backing the database.
         * @param filesPrefix The name of the prefix. The form is amazon_reviews_us_Electronics_v1_00,
         *                    which will expect the two files amazon_reviews_us_Electronics_v1_00.bin and
         *                    amazon_reviews_us_Electronics_v1_00_keyword_index.bin 
         */
        amazon(const std::string& directory, const std::string& filesPrefix);

        /**
         * Predicate Method: good
         * ----------------------
         * Returns true if and only if the database opened without incident.
         * amazon::good would typically return false if:
         *
         *     1.) either one or both of the data files supporting the database were missing
         *     2.) the directory passed to the constructor doesn't exist.
         *     3.) the directory and files all exist, but you don't have the permission to read them.
         */

        bool good() const;


        /**
         * Method: searchKeywordIndex
         * --------------------
         * Searches the amazon keyword index with the query, and populates the reviewIndexes
         * vector with the results. If the search produces no results, the vector is cleared,
         * and its size is left at 0.
         *
         * @param query A query, e.g., 'tv "broke quickly"'
         * @param reviewIndexes A reference to the vector of indexes that will hold
         *                      the resulting indexes from the query. If the query produces no results, 
         *                      reviewIndexes should be cleared and resized to a length of 0.
         *
         * @return true if and only if the query returns at least one matching index 
         */

        bool searchKeywordIndex(const std::string& query, std::vector<unsigned int> &reviewIndexes) const;
        
        
        /**
         * Method: getReview
         * --------------------
         * Given a single index into the reviews database, populates review with the
         * information from the review.
         *
         * @param index The index of the query in the reviews database 
         * @param review A reference to the Review that should be populated
         *        
         * @return true if and only if the review database contains a review at index 
         */
       
        bool getReview(unsigned int index, Review &review) const;
        
        
        /**
         * Method: getSortedReviewsFromIndexes 
         * --------------------
         * Populates an empty reviews vector with the reviews from the indexes 
         * in reviewIndexes, and sorts them, based on the comparison function passed in.
         *
         * @param reviewIndexes A reference to a vector of indexes into the reviews database 
         * @param reviews A reference to a vector of Reviews that will be populated
         * @param cmp The comparison function to compare Reviews 
         *        
         * @return none 
         */
       
        void getSortedReviewsFromIndexes(const std::vector<unsigned int> &reviewIndexes,
            std::vector<Review> &reviews,
            std::function<bool(const Review &, const Review &)> cmp) const;


        /**
         * Method: totalKeywords
         * --------------------
         * Returns the total number of keywords in the keyword database 
         *
         * @return the number of keywords 
         */

        unsigned int totalKeywords() const { return *(unsigned int *)keywordIndexFile; }


        /**
         * Method: totalReviews
         * --------------------
         * Returns the total number of reviews in the database 
         *
         * @return the number of reviews
         */

        unsigned int totalReviews() const { return *(unsigned int *)databaseFile; }


        /** Destructor: ~amazon
         *  -------------------
         *  Releases all resources associated with the amazon database.
         */
        ~amazon();

    private:
        const void *databaseFile;
        const void *keywordIndexFile;

        /** Method: findKeywordPtr
         *  -------------------
         *  Perform binary search to find a pointer to the position in memory containing the information for a particular keyword. Return nullptr if keyword not found in database.
         */
        const char * findKeywordPtr(const std::string& keyword) const;
     
        /** Method: findReviewsMatchingTerm 
         *  -------------------
         *  Iteratively find all reviews containing a term.
            @return A std::set of all review indices containing the term 
         */ 
        std::set<unsigned int> findReviewsContainingTerm(const std::vector<std::string>& term) const;


        /** everything below here needn't be touched.
         *  you're free to investigate, but it's not needed to complete the assignment.
         */
        struct fileInfo {
            int fd;
            size_t fileSize;
            const void *fileMap;
        } databaseInfo, keywordIndexInfo;

        static const void *acquireFileMap(const std::string& fileName, struct fileInfo& info);
        static void releaseFileMap(struct fileInfo& info);

        amazon(const amazon& original) = delete;
        amazon& operator=(const amazon& rhs) = delete;
        amazon& operator=(const amazon& rhs) const = delete;
};
