#include <iostream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include "amazon.h"
using namespace std;

const string kAmazonDataDirectory("/usr/class/archive/cs/cs110/cs110.1204/samples/assign1");
const string kFilesPrefix("amazon_reviews_us_Electronics_v1_00");
static const int kDatabaseNotFound = 2;

static void showUsage(string name)
{
    cout << "Usage: " << name << " <option(s)> index" << endl
        << "Options:\n" << endl
        << "\t-h,--help\t\tShow this help message" << endl
        << "\t-d,--directory DIRECTORY\tSpecify the directory for the database files" << endl
        << "\t-f,--files-prefix FILE_PREFIX\tSpecify the files prefix (default is 'amazon_reviews_us_Electronics_v1_00')" << endl;
}

static int parseArgs(int argc, char **argv, bool &interactive, string &amazonDataDirectory, 
        string &filesPrefix, int &index) {
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if ((arg == "-h") || (arg == "--help")) {
            showUsage(argv[0]);
            return -1;
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
            index = stoi(argv[i]);
        }
    }
    if (!interactive && index == -1) {
        cout << "No index found. Going into interactive mode" << endl;
        interactive = true;
    }
    return 0;
}

int main(int argc, char **argv) {
    int index = -1;
    bool interactive = false;
    string amazonDataDirectory = kAmazonDataDirectory;
    string filesPrefix = kFilesPrefix;

    if (parseArgs(argc, argv, interactive, amazonDataDirectory, filesPrefix, index) == -1) return -1;
    amazon db(amazonDataDirectory, filesPrefix);
    if (!db.good()) {
        cerr << "Problem reading data files...aborting!" << endl; 
        return kDatabaseNotFound;
    }

    cout << "Total number of reviews: " << db.totalReviews() << endl;

    while (true) {
        if (interactive) {
            cout << "Please enter an index (<enter> to end): " << flush;
            string userInput;
            getline(cin, userInput);
            if (userInput == "") break;
            index = stoi(userInput);
        }

        Review review;

        cout << "Getting review at index " << index << endl << endl;
        if (!db.getReview(index, review)) {
            cout << "Could not find a review at index " << index << "." << endl;
        } else {
            cout << review << endl;
        }
        if (!interactive) {
            break;
        }
    }
    return 0;
}
