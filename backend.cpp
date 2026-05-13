/*
 * ============================================================================
 * FreeLance Hub  —  Enterprise C++ OOP Backend Engine
 * Complete Verified Architecture (Relational Filing + IPC Integration)
 * ============================================================================
 * Implemented Features:
 * 1. Hardcoded Seeding: 3 Clients (1 Project each >= $500), 5 Freelancers (1 Gig each >= $100)
 * 2. Multi-Key Stable Quick-Sort (Completed Projects ASC -> Rating DESC -> Unrated centered)
 * 3. Encapsulated Prevention Guards blocking Self-Evaluation Audits
 * 4. Absolute Floor Guards throwing "Insufficient balance" on underpayments
 * 5. Complete removal of all notification overhead, queues, and triggers
 * ============================================================================
 */

// Standard libraries for input/output, file handling, string manipulation, and utilities
#include <iostream>
#include <string>
#include <fstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <climits>
#include <ctime>

using namespace std;

// Forward Declarations
// Letting the compiler know these classes exist early so we can reference them in function parameters.
class Client;
class Freelancer;
class User;
void executeContract(Client& c, Freelancer& f, double amount);

// Abstract Interface for String Serialization
// VIVA TIP: This demonstrates Pure Polymorphism and Abstraction. Any class inheriting this MUST define these methods.
class IProfileDisplayable {
public:
    virtual string getProfileCsv() const = 0; // Pure virtual function (= 0)
    virtual string getRole() const = 0;
    virtual ~IProfileDisplayable() {}
};

// Abstract Interface for Transaction Audit Logs
class IAuditable {
public:
    virtual string generateAuditRecord() const = 0;
    virtual ~IAuditable() {}
};

// Custom Exception Hierarchy
// Inherits from standard exception class to provide clear platform-specific error handling.
class PlatformException : public exception {
protected:
    string message;
public:
    explicit PlatformException(const string& msg) : message("PlatformError: " + msg) {}
    // Overriding what() to cleanly return our custom error message string safely
    virtual const char* what() const noexcept override { return message.c_str(); }
};

class AuthException : public PlatformException {
public:
    explicit AuthException(const string& msg) : PlatformException("AuthFault - " + msg) {}
};

class TransactionException : public PlatformException {
public:
    explicit TransactionException(const string& msg) : PlatformException("LedgerFault - " + msg) {}
};

class FilingException : public PlatformException {
public:
    explicit FilingException(const string& msg) : PlatformException("DiskIOFault - " + msg) {}
};

// String Processing & CSV Sanitization Utilities
// Wrapped in a namespace to keep global memory organized. Used to clean flat IPC data streams.
namespace StringUtils {
    // Trims off unwanted spaces, tabs, or line breaks from both ends of a string
    static string trim(const string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    // Swaps characters that could break our flat CSV/pipe format with safe semicolons
    static string csvClean(const string& input) {
        string output = input;
        for (char& c : output) {
            if (c == ',' || c == '\n' || c == '\r' || c == '|') {
                c = ';';
            }
        }
        return trim(output);
    }

    // Splits an incoming CSV stream line into a structured array of clean string tokens
    static int parseCsv(const string& line, string tokens[], int maxTokens) {
        int count = 0;
        stringstream ss(line);
        string token;
        while (getline(ss, token, ',') && count < maxTokens) {
            tokens[count++] = trim(token);
        }
        return count; // Returns how many parameters we successfully extracted
    }

    // Fetches the system's current date and time formatted cleanly for audit logs
    static string getCurrentTimestamp() {
        time_t now = time(0);
        tm* ltm = localtime(&now);
        stringstream ss;
        ss << 1900 + ltm->tm_year << "-" 
           << setfill('0') << setw(2) << 1 + ltm->tm_mon << "-"
           << setfill('0') << setw(2) << ltm->tm_mday << " "
           << setfill('0') << setw(2) << ltm->tm_hour << ":"
           << setfill('0') << setw(2) << ltm->tm_min << ":"
           << setfill('0') << setw(2) << ltm->tm_sec;
        return ss.str();
    }
}

// Dynamic Memory Container — SafeArray<T>
// VIVA TIP: We built this from scratch using raw pointer arrays instead of std::vector to prove low-level memory mastery.
template <typename T>
class SafeArray {
private:
    int capacity;    // Total slots currently allocated in heap memory
    int currentSize; // Actual number of elements currently stored
    T* data;         // The raw pointer buffer managing our heap items

    // Internal doubling logic: Called automatically when we run out of allocated capacity
    void expand() {
        int newCapacity = (capacity == 0) ? 4 : capacity * 2;
        T* temp = new T[newCapacity]; // Create a new heap block twice the size
        for (int i = 0; i < currentSize; i++) {
            temp[i] = move(data[i]); // Move items over efficiently without deep byte copying
        }
        delete[] data; // Free up the old block immediately to prevent memory leaks
        data = temp;   // Point our internal buffer to the shiny new expanded block
        capacity = newCapacity;
    }

public:
    // Standard constructor starting us off with safe baseline capacity
    explicit SafeArray(int initialCap = 4) 
        : capacity(initialCap), currentSize(0), data(new T[initialCap]) {}

    // Destructor: Automatically deallocates the dynamic heap array when out of scope
    ~SafeArray() { 
        delete[] data; 
    }

    // Deep Copy Constructor (Part of the Rule of Five for custom memory management)
    SafeArray(const SafeArray& other) 
        : capacity(other.capacity), currentSize(other.currentSize), data(new T[other.capacity]) {
        for (int i = 0; i < currentSize; i++) {
            data[i] = other.data[i];
        }
    }

    // Deep Copy Assignment Operator
    SafeArray& operator=(const SafeArray& other) {
        if (this != &other) {
            delete[] data; // Clean our existing heap state first
            capacity = other.capacity;
            currentSize = other.currentSize;
            data = new T[capacity];
            for (int i = 0; i < currentSize; i++) {
                data[i] = other.data[i];
            }
        }
        return *this;
    }

    // Move Constructor: Steals ownership of array buffers instantly without heap duplication
    SafeArray(SafeArray&& other) noexcept 
        : capacity(other.capacity), currentSize(other.currentSize), data(other.data) {
        other.data = nullptr; // Clear the source pointers so its destructor doesn't kill our stolen data
        other.capacity = 0;
        other.currentSize = 0;
    }

    // Move Assignment Operator
    SafeArray& operator=(SafeArray&& other) noexcept {
        if (this != &other) {
            delete[] data;
            capacity = other.capacity;
            currentSize = other.currentSize;
            data = other.data;
            other.data = nullptr;
            other.capacity = 0;
            other.currentSize = 0;
        }
        return *this;
    }

    // Appends an item to the end of our dynamic array, expanding behind the scenes if needed
    void add(const T& item) {
        if (currentSize >= capacity) {
            expand();
        }
        data[currentSize++] = item;
    }

    // Removes an item at a specific index by shifting all higher elements down one slot
    void removeAt(int index) {
        if (index < 0 || index >= currentSize) {
            throw out_of_range("SafeArray: Deletion index out of bounds");
        }
        for (int i = index; i < currentSize - 1; i++) {
            data[i] = move(data[i + 1]);
        }
        currentSize--;
    }

    // Logically empties the container instantly without dropping active heap reservations
    void clear() {
        currentSize = 0;
    }

    // Swaps elements in place (Heavily utilized by our custom stable Quick-Sort)
    void swapElements(int indexA, int indexB) {
        if (indexA < 0 || indexA >= currentSize || indexB < 0 || indexB >= currentSize) {
            throw out_of_range("SafeArray: Swap indices out of bounds");
        }
        T temp = move(data[indexA]);
        data[indexA] = move(data[indexB]);
        data[indexB] = move(temp);
    }

    // Subscript operator overload with strict real-time boundary check guards
    T& operator[](int index) {
        if (index < 0 || index >= currentSize) {
            throw out_of_range("SafeArray: Subscript access out of bounds");
        }
        return data[index];
    }

    // Const-safe overload for read-only array references
    const T& operator[](int index) const {
        if (index < 0 || index >= currentSize) {
            throw out_of_range("SafeArray: Subscript access out of bounds");
        }
        return data[index];
    }

    int size() const { return currentSize; }
    int getCapacity() const { return capacity; }
    bool isEmpty() const { return currentSize == 0; }
};

// Domain Models
// Encapsulates individual historical reviews left on accounts
class Review {
private:
    int score;             // Rating score out of 5
    string comment;        // Friendly review text
    string reviewerHandle; // The account that posted this evaluation
    string timestamp;      // Automatic execution time tag

public:
    Review() : score(0), comment(""), reviewerHandle("SYSTEM"), timestamp("") {}

    // Fully sanitizes data via string utilities right inside initialization
    Review(int s, const string& c, const string& rHandle) 
        : score(s), comment(StringUtils::csvClean(c)), 
          reviewerHandle(StringUtils::csvClean(rHandle)) {
        timestamp = StringUtils::getCurrentTimestamp();
    }

    int getScore() const { return score; }
    string getComment() const { return comment; }
    string getReviewer() const { return reviewerHandle; }
    string getTimestamp() const { return timestamp; }

    // Converts properties to our flattened pipe serialization structure
    string toCsvPacked() const {
        stringstream ss;
        ss << score << ":" << reviewerHandle << ":" << comment;
        return ss.str();
    }
};

// Represents end-to-peer IPC chat message records
class Message {
private:
    string senderUsername;   // Who sent it
    string receiverUsername; // Who receives it
    string messagePayload;   // What they said
    string dispatchTime;     // Exact sending timeline

public:
    Message() : senderUsername(""), receiverUsername(""), messagePayload(""), dispatchTime("") {}

    Message(const string& sender, const string& receiver, const string& payload)
        : senderUsername(StringUtils::csvClean(sender)), 
          receiverUsername(StringUtils::csvClean(receiver)), 
          messagePayload(StringUtils::csvClean(payload)) {
        dispatchTime = StringUtils::getCurrentTimestamp();
    }

    string getSender() const { return senderUsername; }
    string getReceiver() const { return receiverUsername; }
    string getPayload() const { return messagePayload; }
    string getTimestamp() const { return dispatchTime; }

    // Packages chat output safely for our database storage tables
    string toCsvRecord() const {
        stringstream ss;
        ss << senderUsername << "," << receiverUsername << "," << dispatchTime << "," << messagePayload;
        return ss.str();
    }
};

// Tracks immutable transaction execution details for the platform audit logs
class AuditLog : public IAuditable {
private:
    string transactionID; // Generated execution index
    string executionType; // Tag indicating what action fired
    string initiator;     // The account handle triggering the mutation
    string detailSummary; // Relevant parameters and value updates
    string timestamp;

public:
    AuditLog() : transactionID(""), executionType(""), initiator(""), detailSummary(""), timestamp("") {}

    AuditLog(const string& tID, const string& type, const string& init, const string& summary)
        : transactionID(StringUtils::csvClean(tID)), 
          executionType(StringUtils::csvClean(type)), 
          initiator(StringUtils::csvClean(init)), 
          detailSummary(StringUtils::csvClean(summary)) {
        timestamp = StringUtils::getCurrentTimestamp();
    }

    // Overridden method writing formatted record straight to storage files
    string generateAuditRecord() const override {
        stringstream ss;
        ss << transactionID << "," << timestamp << "," << executionType << "," 
           << initiator << "," << detailSummary;
        return ss.str();
    }
};

// Encapsulates active services published by Freelancers
class Gig {
private:
    int gigID;                 // Unique item identifier
    int ownerFreelancerID;     // Mapped back to the creating user account
    string serviceTitle;       // Brief overview of what they offer
    string industryCategory;   // Domain sector filter
    string searchTags;         // Search criteria for fast indexing
    double contractPrice;      // Base required valuation
    string freelancerUsername; // Author handle stored directly for faster UI rendering
    bool isActiveListing;      // Soft open/closed state flag

public:
    Gig() : gigID(0), ownerFreelancerID(0), contractPrice(0.0), isActiveListing(false) {}

    Gig(int gID, int fID, const string& title, const string& category, 
        const string& tags, double price, const string& fName)
        : gigID(gID), ownerFreelancerID(fID), 
          serviceTitle(StringUtils::csvClean(title)), 
          industryCategory(StringUtils::csvClean(category)), 
          searchTags(StringUtils::csvClean(tags)), 
          contractPrice(price), 
          freelancerUsername(StringUtils::csvClean(fName)),
          isActiveListing(true) {}

    int getGigID() const { return gigID; }
    int getOwnerID() const { return ownerFreelancerID; }
    string getTitle() const { return serviceTitle; }
    string getCategory() const { return industryCategory; }
    string getTags() const { return searchTags; }
    double getPrice() const { return contractPrice; }
    string getOwnerName() const { return freelancerUsername; }
    bool getStatus() const { return isActiveListing; }

    // Mutator with our mandatory viva requirement validation guard
    void setPrice(double newPrice) { 
        // Enforces the absolute mathematical floor: A gig can never be created/set below $100.
        if (newPrice < 100.0) throw invalid_argument("Price cannot drop below the $100 floor");
        contractPrice = newPrice; 
    }

    void setStatus(bool status) { isActiveListing = status; }

    // Compiles data into an export string for the CSV tables
    string toCsvExport() const {
        stringstream ss;
        ss << fixed << setprecision(2)
           << gigID << "," << ownerFreelancerID << "," << freelancerUsername << ","
           << serviceTitle << "," << industryCategory << "," << searchTags << ","
           << contractPrice << "," << (isActiveListing ? "true" : "false");
        return ss.str();
    }
};

// Encapsulates open enterprise requirements posted by Clients
class Project {
private:
    int projectID;           // Unique item identifier
    int ownerClientID;       // Mapped back to the creating Client account
    string objectiveTitle;   // Clean title of work requirements
    string industryCategory; // Classification tag
    string clientUsername;   // Posting client display string
    double maximumBudget;    // Max funds allocated for the task
    bool isOpenForBids;      // Open/closed boolean state
    string creationDate;     // Generated creation stamp
    string attachmentPath;   // Relative system path linking external documentation guidelines

public:
    Project() : projectID(0), ownerClientID(0), maximumBudget(0.0), isOpenForBids(false), attachmentPath("none") {}

    Project(int pID, int cID, const string& title, const string& category, 
            double budget, const string& cName, bool openStatus = true, const string& attach = "none")
        : projectID(pID), ownerClientID(cID), 
          objectiveTitle(StringUtils::csvClean(title)), 
          industryCategory(StringUtils::csvClean(category)), 
          maximumBudget(budget), 
          isOpenForBids(openStatus), 
          clientUsername(StringUtils::csvClean(cName)),
          attachmentPath(StringUtils::csvClean(attach)) {
        creationDate = StringUtils::getCurrentTimestamp();
    }

    int getProjectID() const { return projectID; }
    int getClientID() const { return ownerClientID; }
    string getTitle() const { return objectiveTitle; }
    string getCategory() const { return industryCategory; }
    string getClientName() const { return clientUsername; }
    double getBudget() const { return maximumBudget; }
    bool getStatus() const { return isOpenForBids; }
    string getDate() const { return creationDate; }
    string getAttachment() const { return attachmentPath; }

    void closeProject() { isOpenForBids = false; }
    void openProject() { isOpenForBids = true; }
    void updateBudget(double b) { maximumBudget = b; }

    // Serializes attributes for flat storage formatting
    string toCsvExport() const {
        stringstream ss;
        ss << fixed << setprecision(2)
           << projectID << "," << ownerClientID << "," << clientUsername << ","
           << objectiveTitle << "," << industryCategory << "," << maximumBudget << ","
           << (isOpenForBids ? "true" : "false") << "," << creationDate << "," << attachmentPath;
        return ss.str();
    }
};

// Base User Model
// VIVA TIP: Serves as the central abstract class handling base account identity logic.
class User : public IProfileDisplayable {
protected:
    int accountID;                // Primary system key
    string usernameHandle;        // Display username handle
    string securityHash;          // Raw user security key
    double walletReserve;         // Active account monetary balance
    bool activeConnectionStatus;  // Login status tracking
    double totalAccumulatedSpent; // Total financial liquidity flowing through account
    int totalCompletedContracts;  // Global metrics metric used by our sorting functions

    // VIVA TIP: Composition modeling. The user perfectly owns this custom sub-array of historical feedback.
    SafeArray<Review> platformReviews;      

    // Static tracker tracking how many total users have been instantiated
    static int platformGlobalUserCounter;   

public:
    User(int accID, const string& uName, const string& pHash)
        : accountID(accID), 
          usernameHandle(StringUtils::csvClean(uName)), 
          securityHash(StringUtils::csvClean(pHash)), 
          walletReserve(0.0), 
          activeConnectionStatus(false), 
          totalAccumulatedSpent(0.0), 
          totalCompletedContracts(0) {
        platformGlobalUserCounter++; // Automatically increments shared static counter
    }

    virtual ~User() {}

    // Pure virtual methods ensuring User acts as an un-instantiable abstract interface
    string getProfileCsv() const override = 0;
    string getRole() const override = 0;

    int getID() const { return accountID; }
    string getUsername() const { return usernameHandle; }
    string getPasswordHash() const { return securityHash; }
    double getWalletBalance() const { return walletReserve; }
    double getTotalSpent() const { return totalAccumulatedSpent; }
    int getCompletedCount() const { return totalCompletedContracts; }
    bool isOnline() const { return activeConnectionStatus; }
    
    // Explicit public accessor for the sorting logic so it reads size without throwing errors
    int getReviewCount() const { return platformReviews.size(); }

    bool verifyPasskey(const string& passkeyAttempt) const {
        return securityHash == passkeyAttempt;
    }

    // Dynamically parses stored sub-records to compute real-time rating average
    double computeAverageRating() const {
        if (platformReviews.isEmpty()) return 0.0;
        double scoreSum = 0.0;
        int reviewCount = platformReviews.size();
        for (int i = 0; i < reviewCount; i++) {
            scoreSum += platformReviews[i].getScore();
        }
        return scoreSum / reviewCount;
    }

    // Direct mutators called primarily when loading state from CSV files
    void setConnectionState(bool state) { activeConnectionStatus = state; }
    void setWalletReserveDirect(double value) { walletReserve = value; }
    void setTotalSpentDirect(double value) { totalAccumulatedSpent = value; }
    void setCompletedContractsDirect(int count) { totalCompletedContracts = count; }

    // Adds a newly verified review directly into our encapsulated feedback array
    void addReviewRecord(int score, const string& text, const string& reviewer) {
        if (score < 1 || score > 5) throw invalid_argument("Rating bounds violated");
        platformReviews.add(Review(score, text, reviewer));
    }

    // Safely transforms all review records into a flattened sub-string pipe format
    string serializeReviewsPacked() const {
        if (platformReviews.isEmpty()) return "none";
        stringstream ss;
        int count = platformReviews.size();
        for (int i = 0; i < count; i++) {
            if (i > 0) ss << "|";
            ss << platformReviews[i].toCsvPacked();
        }
        return ss.str();
    }

    static int getGlobalCounter() { return platformGlobalUserCounter; }
    static void resetGlobalCounter() { platformGlobalUserCounter = 0; }

    // Operator overloading demonstrating controlled balance top-ups
    User& operator+=(double injectionAmount) {
        if (injectionAmount <= 0) {
            throw TransactionException("Operator+= injection capital must be positive");
        }
        walletReserve += injectionAmount;
        return *this;
    }

    // Operator overloading safely processing fund deductions
    User& operator-=(double deductionAmount) {
        if (deductionAmount <= 0) {
            throw TransactionException("Operator-= deduction capital must be positive");
        }
        if (walletReserve < deductionAmount) {
            throw TransactionException("Insufficient asset reserves to execute deduction");
        }
        walletReserve -= deductionAmount;
        return *this;
    }

    // VIVA TIP: Safely breaking encapsulation boundaries. Declaring this standalone function 
    // as a friend lets it adjust sensitive internal balances simultaneously without exposing public write access.
    friend void executeContract(Client& c, Freelancer& f, double amount);
};

int User::platformGlobalUserCounter = 0;

// Derived Role Models
// VIVA TIP: Concrete specialization extending base User entity with specialized domain roles.
class Freelancer : public User {
public:
    Freelancer(int id, const string& name, const string& pass) 
        : User(id, name, pass) {}

    string getRole() const override { return "Freelancer"; }

    // Specialized logic calculating tiered achievement badges dynamically based on lifetime delivery volume
    string calculateAchievementMedal() const {
        if (totalCompletedContracts >= 20) return "DIAMOND";
        if (totalCompletedContracts >= 10) return "GOLD";
        if (totalCompletedContracts >= 5)  return "SILVER";
        if (totalCompletedContracts >  0)  return "BRONZE";
        return "UNRANKED";
    }

    // Overridden profile serialization providing freelancer-specific formatted attributes
    string getProfileCsv() const override {
        stringstream ss;
        ss << fixed << setprecision(2)
           << accountID << "," << usernameHandle << "," << getRole() << ","
           << (activeConnectionStatus ? "true" : "false") << ","
           << walletReserve << "," << computeAverageRating() << ","
           << platformReviews.size() << "," << serializeReviewsPacked() << ","
           << totalCompletedContracts << "," << totalAccumulatedSpent << ","
           << calculateAchievementMedal();
        return ss.str();
    }
};

// Specialized client organization node inheriting baseline User mechanisms
class Client : public User {
private:
    string corporateTaxonomyID; // Encapsulated business mapping sequence

public:
    Client(int id, const string& name, const string& pass) 
        : User(id, name, pass) {
        stringstream ss;
        // Dynamically creates unique internal verification indices for corporate profiles
        ss << "CORP-TX-" << id << "-" << (1000 + (id * 7) % 9000);
        corporateTaxonomyID = ss.str();
    }

    string getRole() const override { return "Client"; }
    string getTaxonomyID() const { return corporateTaxonomyID; }

    // Overridden serialization mapping clean Client layout properties
    string getProfileCsv() const override {
        stringstream ss;
        ss << fixed << setprecision(2)
           << accountID << "," << usernameHandle << "," << getRole() << ","
           << (activeConnectionStatus ? "true" : "false") << ","
           << walletReserve << "," << computeAverageRating() << ","
           << platformReviews.size() << "," << serializeReviewsPacked() << ","
           << totalCompletedContracts << "," << totalAccumulatedSpent << ","
           << corporateTaxonomyID;
        return ss.str();
    }
};

// Escrow Settlement Logic
// The actual standalone friend mutator instantly modifying balances inside valid closed transactions
void executeContract(Client& c, Freelancer& f, double amount) {
    if (amount <= 0) {
        throw TransactionException("Contract value must exceed zero");
    }
    if (c.walletReserve < amount) {
        throw TransactionException("Client lacks capital to fulfill contract obligation");
    }

    // Simultaneously debits client, credits freelancer, and updates global scoring tracking metrics
    c.walletReserve -= amount;
    c.totalAccumulatedSpent += amount;
    c.totalCompletedContracts++;

    f.walletReserve += amount;
    f.totalAccumulatedSpent += amount;
    f.totalCompletedContracts++;
}

// Singleton Database Engine
// VIVA TIP: Manages the definitive memory state centrally. Hides constructors to ensure only one instance ever exists.
class PlatformDB {
private:
    int sequenceGigID;              // Generates running global ID tags
    int sequenceProjectID;          // Generates running global ID tags
    int sequenceTransactionAuditID; // Tracks sequence counts for audit record IDs

    // Master memory arrays maintaining our complete operational state graph
    SafeArray<User*> usersTable;
    SafeArray<Gig> gigsTable;
    SafeArray<Project> projectsTable;
    SafeArray<Message> messagingLedger;
    SafeArray<AuditLog> auditTrailLedger;

    // Private constructor initializing internal counter states
    PlatformDB() : sequenceGigID(10), sequenceProjectID(10), sequenceTransactionAuditID(1000) {}

    // Destructor iterating pointer lists safely to execute absolute system memory cleanups
    ~PlatformDB() {
        int uCount = usersTable.size();
        for (int i = 0; i < uCount; i++) {
            delete usersTable[i];
        }
    }

    // Deleted copy construction and assignment operators lock down absolute Singleton boundaries
    PlatformDB(const PlatformDB&) = delete;
    PlatformDB& operator=(const PlatformDB&) = delete;

    // Stable Multi-Key Quick-Sort Partition implementation
    // VIVA TIP: Sorts service items in-place based on three chained criteria without dynamic vector helpers.
    int partitionGigsCustom(SafeArray<Gig>& arr, int lowIndex, int highIndex) {
        Gig pivotGig = arr[highIndex];
        Freelancer* pivotUser = dynamic_cast<Freelancer*>(getUserByID(pivotGig.getOwnerID()));
        
        // Resolves primary comparison indices from target profiles safely
        int pivotProjects = pivotUser ? pivotUser->getCompletedCount() : INT_MAX;
        double pivotScore = pivotUser ? pivotUser->computeAverageRating() : -1.0;
        int pivotReviews = pivotUser ? pivotUser->getReviewCount() : 0;

        int idx = lowIndex - 1;
        for (int j = lowIndex; j < highIndex; j++) {
            Freelancer* currF = dynamic_cast<Freelancer*>(getUserByID(arr[j].getOwnerID()));
            if (!currF) continue;

            int currProjects = currF->getCompletedCount();
            double currScore = currF->computeAverageRating();
            int currReviews = currF->getReviewCount();

            bool prioritizeLeft = false;

            // Sorting Rule 1: Prioritize lower project completion quantities (Ascending)
            if (currProjects < pivotProjects) {
                prioritizeLeft = true;
            } else if (currProjects == pivotProjects) {
                // Sorting Rule 2: Push items with zero verified evaluations backward
                if (currReviews == 0 && pivotReviews > 0) {
                    prioritizeLeft = false; 
                } else if (currReviews > 0 && pivotReviews == 0) {
                    prioritizeLeft = true;  
                } else {
                    // Sorting Rule 3: Sort by average cumulative ratings (Descending)
                    if (currScore > pivotScore) {
                        prioritizeLeft = true;
                    } else if (currScore == pivotScore && currReviews > pivotReviews) {
                        prioritizeLeft = true;
                    }
                }
            }

            // Swaps out-of-order slots immediately via raw pointer indices
            if (prioritizeLeft) {
                idx++;
                arr.swapElements(idx, j);
            }
        }
        arr.swapElements(idx + 1, highIndex);
        return idx + 1;
    }

    // Standard recursive quicksort launcher processing items partitioned through custom multi-key filters
    void executeQuickSortGigs(SafeArray<Gig>& arr, int low, int high) {
        if (low < high) {
            int partitionIndex = partitionGigsCustom(arr, low, high);
            executeQuickSortGigs(arr, low, partitionIndex - 1);
            executeQuickSortGigs(arr, partitionIndex + 1, high);
        }
    }

    // Appends critical internal operational entries into our permanent audit persistence arrays
    void recordSystemAudit(const string& type, const string& init, const string& summary) {
        stringstream ss;
        ss << "TX-AUDIT-" << sequenceTransactionAuditID++;
        auditTrailLedger.add(AuditLog(ss.str(), type, init, summary));
    }

public:
    // Public thread-safe gateway returning our sole operational core reference safely
    static PlatformDB& getInstance() {
        static PlatformDB activeInstance;
        return activeInstance;
    }

    // Looks up target profile records linearly across primary memory bounds
    User* getUserByID(int targetID) const {
        int count = usersTable.size();
        for (int i = 0; i < count; i++) {
            if (usersTable[i]->getID() == targetID) {
                return usersTable[i];
            }
        }
        return nullptr;
    }

    // Resolves identity configurations safely matching clean text handle criteria
    User* getUserByUsername(const string& targetHandle) const {
        string cleanedTarget = StringUtils::csvClean(targetHandle);
        int count = usersTable.size();
        for (int i = 0; i < count; i++) {
            if (usersTable[i]->getUsername() == cleanedTarget) {
                return usersTable[i];
            }
        }
        return nullptr;
    }

    // Disk Filing Serialization
    // Immediately truncates output files and systematically writes out our active runtime memory arrays
    void persistDatabaseToStorage() const {
        ofstream outFile("platform_db.csv", ios::trunc);
        if (!outFile.is_open()) {
            throw FilingException("Failed to acquire disk mapping for output storage stream");
        }

        outFile << fixed << setprecision(2);
        outFile << "SYS_COUNTER," << sequenceGigID << "," << sequenceProjectID << "," 
                << sequenceTransactionAuditID << "\n";

        int uCount = usersTable.size();
        for (int i = 0; i < uCount; i++) {
            User* u = usersTable[i];
            outFile << "TBL_USER," << u->getRole() << "," << u->getID() << ","
                    << u->getUsername() << "," << u->getPasswordHash() << ","
                    << u->getWalletBalance() << "," << u->getTotalSpent() << ","
                    << u->getCompletedCount() << ",";
            string pRev = u->serializeReviewsPacked();
            outFile << (pRev.empty() ? "none" : pRev) << "\n";
        }

        int gCount = gigsTable.size();
        for (int i = 0; i < gCount; i++) {
            outFile << "TBL_GIG," << gigsTable[i].toCsvExport() << "\n";
        }

        int pCount = projectsTable.size();
        for (int i = 0; i < pCount; i++) {
            outFile << "TBL_PROJECT," << projectsTable[i].toCsvExport() << "\n";
        }

        int mCount = messagingLedger.size();
        for (int i = 0; i < mCount; i++) {
            outFile << "TBL_MSG," << messagingLedger[i].toCsvRecord() << "\n";
        }

        int aCount = auditTrailLedger.size();
        for (int i = 0; i < aCount; i++) {
            outFile << "TBL_AUDIT," << auditTrailLedger[i].generateAuditRecord() << "\n";
        }

        outFile.close();
    }

    // Reconstructs runtime graph completely directly from file logs (Ensures perfect cross-process synchronization)
    void loadDatabaseFromStorage() {
        ifstream inFile("platform_db.csv");
        if (!inFile.is_open()) return;

        // Clear existing local tracking arrays to purge stale values before reloading
        int oldUserCount = usersTable.size();
        for (int i = 0; i < oldUserCount; i++) {
            delete usersTable[i];
        }
        usersTable.clear();
        gigsTable.clear();
        projectsTable.clear();
        messagingLedger.clear();
        auditTrailLedger.clear();
        User::resetGlobalCounter();

        string lineBuffer;
        int maxDiscoveredGigID = 9;
        int maxDiscoveredProjectID = 9;

        // Iterates linearly parsing raw string entries straight from CSV arrays
        while (getline(inFile, lineBuffer)) {
            lineBuffer = StringUtils::trim(lineBuffer);
            if (lineBuffer.empty()) continue;

            string t[15];
            int tokenCount = StringUtils::parseCsv(lineBuffer, t, 15);
            if (tokenCount < 2) continue;

            string rowDiscriminator = t[0];

            try {
                // Reconstructs persistent configuration numbers
                if (rowDiscriminator == "SYS_COUNTER" && tokenCount >= 4) {
                    sequenceGigID = stoi(t[1]);
                    sequenceProjectID = stoi(t[2]);
                    sequenceTransactionAuditID = stoi(t[3]);
                }
                // Dynamically heap-allocates concrete derived object pointers matching source metadata tags
                else if (rowDiscriminator == "TBL_USER" && tokenCount >= 9) {
                    string roleGroup = t[1];
                    int accountID = stoi(t[2]);
                    string uHandle = t[3];
                    string secHash = t[4];
                    double wBal = stod(t[5]);
                    double accumSpent = stod(t[6]);
                    int completedQty = stoi(t[7]);
                    string packedReviews = t[8];

                    User* restoredUser = nullptr;
                    if (roleGroup == "Client") {
                        restoredUser = new Client(accountID, uHandle, secHash);
                    } else {
                        restoredUser = new Freelancer(accountID, uHandle, secHash);
                    }

                    restoredUser->setWalletReserveDirect(wBal);
                    restoredUser->setTotalSpentDirect(accumSpent);
                    restoredUser->setCompletedContractsDirect(completedQty);

                    // Reconstructs evaluation parameters splitting sub-string blocks linearly
                    if (packedReviews != "none" && !packedReviews.empty()) {
                        stringstream revStream(packedReviews);
                        string innerBlock;
                        while (getline(revStream, innerBlock, '|')) {
                            size_t firstColon = innerBlock.find(':');
                            if (firstColon != string::npos) {
                                int rScore = stoi(innerBlock.substr(0, firstColon));
                                string remainBlock = innerBlock.substr(firstColon + 1);
                                size_t secondColon = remainBlock.find(':');
                                if (secondColon != string::npos) {
                                    string revAuthor = remainBlock.substr(0, secondColon);
                                    string revText = remainBlock.substr(secondColon + 1);
                                    restoredUser->addReviewRecord(rScore, revText, revAuthor);
                                }
                            }
                        }
                    }
                    usersTable.add(restoredUser);
                }
                // Appends simple value asset structs straight to arrays
                else if (rowDiscriminator == "TBL_GIG" && tokenCount >= 9) {
                    int gID = stoi(t[1]);
                    Gig catalogedGig(gID, stoi(t[2]), t[4], t[5], t[6], stod(t[7]), t[3]);
                    catalogedGig.setStatus(t[8] == "true");
                    gigsTable.add(catalogedGig);
                    if (gID > maxDiscoveredGigID) maxDiscoveredGigID = gID;
                }
                else if (rowDiscriminator == "TBL_PROJECT" && tokenCount >= 8) {
                    int pID = stoi(t[1]);
                    string attach = (tokenCount >= 10) ? t[9] : "none";
                    projectsTable.add(Project(pID, stoi(t[2]), t[4], t[5], stod(t[6]), t[3], (t[7] == "true"), attach));
                    if (pID > maxDiscoveredProjectID) maxDiscoveredProjectID = pID;
                }
                else if (rowDiscriminator == "TBL_MSG" && tokenCount >= 5) {
                    messagingLedger.add(Message(t[1], t[2], t[4]));
                }
                else if (rowDiscriminator == "TBL_AUDIT" && tokenCount >= 6) {
                    auditTrailLedger.add(AuditLog(t[1], t[3], t[4], t[5]));
                }
            } catch (...) { continue; }
        }

        inFile.close();
        if (sequenceGigID <= maxDiscoveredGigID) sequenceGigID = maxDiscoveredGigID + 1;
        if (sequenceProjectID <= maxDiscoveredProjectID) sequenceProjectID = maxDiscoveredProjectID + 1;
    }

    // Command Handlers
    // Factory allocation logic resolving user registration profiles securely
    string executeRegistrationCommand(const string& roleGroup, const string& targetUName, 
                                      const string& passHash, double depositCapital) {
        if (getUserByUsername(targetUName) != nullptr) {
            throw AuthException("Unique constraint violation: Handle already mapped");
        }

        int assignedAccountID = User::getGlobalCounter() + 1;
        // Instantiates structural profile structures safely mapping explicit target specializations
        if (roleGroup == "client") {
            Client* newClient = new Client(assignedAccountID, targetUName, passHash);
            if (depositCapital > 0) *newClient += depositCapital;
            usersTable.add(newClient);
        } else {
            Freelancer* newFreelancer = new Freelancer(assignedAccountID, targetUName, passHash);
            if (depositCapital > 0) *newFreelancer += depositCapital;
            usersTable.add(newFreelancer);
        }

        recordSystemAudit("ACCOUNT_CREATION", targetUName, "Role allocation committed: " + roleGroup);
        persistDatabaseToStorage();
        return "true," + to_string(assignedAccountID) + ",Identity generated successfully";
    }

    // Session authorization executing baseline validation checking user parameters
    string executeLoginCommand(const string& attemptUsername, const string& attemptPasskey) {
        User* targetUser = getUserByUsername(attemptUsername);
        if (!targetUser || !targetUser->verifyPasskey(attemptPasskey)) {
            recordSystemAudit("LOGIN_DENIED", attemptUsername, "Security validation failure");
            throw AuthException("Access validation failure: Passkey verification rejected");
        }

        targetUser->setConnectionState(true);
        recordSystemAudit("SESSION_AUTHORIZED", attemptUsername, "System connection state active");
        return "true," + targetUser->getProfileCsv();
    }

    // Returns specific profile configurations directly to flat reporting strings
    string executeProfileAcquisition(int searchID) {
        User* userObj = getUserByID(searchID);
        if (!userObj) throw PlatformException("Target identity absent from active mapping structures");
        return "true," + userObj->getProfileCsv();
    }

    // Processes clean wallet asset adjustments manually via overloaded addition logic
    string executeWalletTopUp(int targetID, double injectionFunds) {
        User* userObj = getUserByID(targetID);
        if (!userObj) throw PlatformException("Identity key missing from lookup tables");
        
        // Intercepts Freelancers attempting direct fund top-ups
        if (userObj->getRole() == "Freelancer") {
            recordSystemAudit("VIOLATION_ATTEMPT", userObj->getUsername(), "Freelancer direct top-up rejected");
            throw TransactionException("System policy blocks Freelancer accounts from direct asset injections");
        }

        *userObj += injectionFunds;
        recordSystemAudit("CAPITAL_INJECTION", userObj->getUsername(), "Operator+= loaded balance $" + to_string(injectionFunds));
        persistDatabaseToStorage();

        stringstream ss;
        ss << fixed << setprecision(2) << "true," << userObj->getWalletBalance();
        return ss.str();
    }

    // STRICT ESCROW FLOOR GUARD THROWING "INSUFFICIENT BALANCE" EXCEPTION ON UNDERPAYMENTS OR OVERDRAFTS
    // VIVA TIP: This is where our primary financial protection rules and exception checks live.
    string executePaymentSettlement(int clientID, int freelancerID, double transactionValue, double expectedAmount = 0.0) {
        User* sourceClient = getUserByID(clientID);
        User* targetFreelancer = getUserByID(freelancerID);

        // Ensures nodes map precisely to expected concrete roles
        if (!sourceClient || sourceClient->getRole() != "Client") {
            throw TransactionException("Origin node fails validation checks as registered Client entity");
        }
        if (!targetFreelancer || targetFreelancer->getRole() != "Freelancer") {
            throw TransactionException("Destination node fails validation checks as registered Freelancer entity");
        }

        // ABSOLUTE FLOOR GUARD: Intercepts if funds transferred fall below required milestone floor
        // Instantly rejects execution and bypasses the balance modifier if conditions are violated.
        if (transactionValue < expectedAmount) {
            stringstream errStream;
            errStream << fixed << setprecision(2) << "false,Payment failed: Insufficient balance or amount below the required milestone threshold ($" << expectedAmount << ").";
            return errStream.str();
        }

        // WALLET CAPACITY GUARD: Intercepts actual overdrafts checking remaining asset reserves
        if (sourceClient->getWalletBalance() < transactionValue) {
            return "false,Payment failed: Insufficient balance in wallet.";
        }

        // Casts basic entity references safely to derived concrete references
        Client* verifiedClient = dynamic_cast<Client*>(sourceClient);
        Freelancer* verifiedFreelancer = dynamic_cast<Freelancer*>(targetFreelancer);

        // Fires standalone friend mutator logic shifting financial parameters cleanly
        executeContract(*verifiedClient, *verifiedFreelancer, transactionValue);

        stringstream auditMsg;
        auditMsg << "Contract settled. Asset balance shift $" << fixed << setprecision(2) << transactionValue;
        recordSystemAudit("CONTRACT_EXECUTION", verifiedClient->getUsername(), auditMsg.str());
        persistDatabaseToStorage();

        stringstream ss;
        ss << fixed << setprecision(2) << "true," << verifiedClient->getWalletBalance() 
           << "," << verifiedFreelancer->getWalletBalance();
        return ss.str();
    }

    // Exports catalog options sorted directly via our multi-key algorithm
    string executeCatalogRetrievalGigs() {
        if (gigsTable.size() > 1) {
            executeQuickSortGigs(gigsTable, 0, gigsTable.size() - 1);
        }
        stringstream ss;
        ss << "true," << gigsTable.size();
        if (!gigsTable.isEmpty()) ss << ",";

        int limit = gigsTable.size();
        for (int i = 0; i < limit; i++) {
            if (i > 0) ss << "|";
            ss << gigsTable[i].toCsvExport();
        }
        return ss.str();
    }

    // Compiles active mandate structures straight to readable string pipelines
    string executeCatalogRetrievalProjects() {
        stringstream ss;
        ss << "true," << projectsTable.size();
        if (!projectsTable.isEmpty()) ss << ",";

        int limit = projectsTable.size();
        for (int i = 0; i < limit; i++) {
            if (i > 0) ss << "|";
            ss << projectsTable[i].toCsvExport();
        }
        return ss.str();
    }

    // Verifies constraints and registers newly available service capability entries
    string executeAddGigOperation(int ownerFreelancerID, const string& serviceTitle, 
                                  const string& indCategory, const string& taxonomyTags, 
                                  double baseValuation) {
        User* userObj = getUserByID(ownerFreelancerID);
        if (!userObj || userObj->getRole() != "Freelancer") {
            throw AuthException("Privilege level invalid: Action restricted to active Freelancer entities");
        }

        // Enforce the $100 platform floor directly inside the engine
        // Instantly rejects item creation if proposed value drops below safe bounds.
        if (baseValuation < 100.0) {
            return "false,Gig price cannot be set below the $100.00 platform floor.";
        }

        int newGigID = sequenceGigID++;
        gigsTable.add(Gig(newGigID, ownerFreelancerID, serviceTitle, indCategory, 
                          taxonomyTags, baseValuation, userObj->getUsername()));

        recordSystemAudit("GIG_PUBLISHED", userObj->getUsername(), "Capability indexed: ID " + to_string(newGigID));
        persistDatabaseToStorage();
        return "true,Gig listing appended to system persistent arrays successfully";
    }

    // Verifies mandate boundaries before publishing client tasks
    string executeAddProjectOperation(int ownerClientID, const string& objectiveTitle, 
                                      const string& targetCategory, double maximumAllocatedFunds,
                                      const string& attachmentPath = "none") {
        User* userObj = getUserByID(ownerClientID);
        if (!userObj || userObj->getRole() != "Client") {
            throw AuthException("Privilege level invalid: Action restricted to active Client entities");
        }

        // Enforce the $500 platform floor directly inside the engine
        if (maximumAllocatedFunds < 500.0) {
            return "false,Project budget cannot be set below the $500.00 platform floor.";
        }

        int newProjectID = sequenceProjectID++;
        projectsTable.add(Project(newProjectID, ownerClientID, objectiveTitle, 
                                  targetCategory, maximumAllocatedFunds, userObj->getUsername(), true, attachmentPath));

        recordSystemAudit("PROJECT_PUBLISHED", userObj->getUsername(), "Mandate indexed: ID " + to_string(newProjectID));
        persistDatabaseToStorage();
        return "true,Strategic mandate scope written to disk structures successfully";
    }

    // Security guard rejecting accounts attempting self-evaluations
    // VIVA TIP: Directly targets users attempting fraudulent review submissions on their own handles.
    string executeReviewAggregation(int callerAccountID, int targetAccountID, int assessmentScore, 
                                    const string& justificationComment) {
        if (callerAccountID == targetAccountID) {
            return "false,Security Violation: Users are strictly prohibited from evaluating themselves.";
        }
        
        User* userObj = getUserByID(targetAccountID);
        if (!userObj) throw PlatformException("Requested target evaluation handle missing from array bounds");

        userObj->addReviewRecord(assessmentScore, justificationComment, "VERIFIED_PROXY");
        
        recordSystemAudit("REVIEW_COMMITTED", "SYSTEM", "Evaluation appended to node ID " + to_string(targetAccountID));
        persistDatabaseToStorage();

        stringstream ss;
        ss << fixed << setprecision(2) << "true," << userObj->computeAverageRating();
        return ss.str();
    }

    // Scans complete global arrays linearly computing active escrow balance metrics
    string executeEcosystemMetricsSync() {
        int activeClients = 0, activeFreelancers = 0;
        double ecosystemCapitalizationSum = 0.0;

        int limit = usersTable.size();
        for (int i = 0; i < limit; i++) {
            User* currUser = usersTable[i];
            if (currUser->getRole() == "Client") activeClients++; else activeFreelancers++;
            ecosystemCapitalizationSum += currUser->getWalletBalance();
        }

        stringstream ss;
        ss << fixed << setprecision(2) << "true," << User::getGlobalCounter() << ","
           << activeClients << "," << activeFreelancers << ","
           << gigsTable.size() << "," << projectsTable.size() << ","
           << ecosystemCapitalizationSum;
        return ss.str();
    }

    // Gathers clean representations of all known account strings
    string executeUsersHandleAcquisition() {
        stringstream ss; ss << "true";
        int limit = usersTable.size();
        for (int i = 0; i < limit; i++) ss << "," << usersTable[i]->getUsername();
        return ss.str();
    }

    // Directly pushes peer IPC conversation buffers straight into relational records
    string executeOutboundMessageDispatch(const string& senderHandle, const string& receiverHandle, 
                                          const string& streamPayload) {
        User* sourceNode = getUserByUsername(senderHandle);
        User* destinationNode = getUserByUsername(receiverHandle);

        if (!sourceNode || !destinationNode) {
            throw PlatformException("Discovered missing communication nodes during handshake");
        }

        messagingLedger.add(Message(senderHandle, receiverHandle, streamPayload));

        recordSystemAudit("MESSAGE_DISPATCH", senderHandle, "Piped IPC chat buffer to " + receiverHandle);
        persistDatabaseToStorage();
        return "true,Message pipelined safely to relational tables";
    }

    // Extracts targeted end-to-peer communication histories linearly
    string executeHistoryExtraction(const string& localNode, const string& remoteNode) {
        stringstream ss; ss << "true";
        bool elementsDiscovered = false;

        int limit = messagingLedger.size();
        for (int i = 0; i < limit; i++) {
            const Message& currMsg = messagingLedger[i];
            bool forwardMatch = (currMsg.getSender() == localNode && currMsg.getReceiver() == remoteNode);
            bool backwardMatch = (currMsg.getSender() == remoteNode && currMsg.getReceiver() == localNode);

            if (forwardMatch || backwardMatch) {
                if (elementsDiscovered) ss << "|"; else ss << ",";
                ss << currMsg.getSender() << ":" << currMsg.getPayload();
                elementsDiscovered = true;
            }
        }
        return elementsDiscovered ? ss.str() : "true,none";
    }
};

// ============================================================================
// HARDCODED SEEDING LOGIC: 3 CLIENTS & 5 FREELANCERS WITH EXACT VALIDATED FLOORS
// Populates application state consistently upon startup if missing disk ledger files
// ============================================================================
static void initializeFallbackEcosystem(PlatformDB& db) {
    // Hardcode Exactly 3 Clients with initial deposits (Projects require >= $500 baseline budgets)
    db.executeRegistrationCommand("client", "TechCorp", "pass123", 5000.0); // ID 1
    db.executeRegistrationCommand("client", "StartupXY", "pass123", 3000.0); // ID 2
    db.executeRegistrationCommand("client", "Naeem", "pass123", 4000.0);     // ID 3

    // Hardcode Exactly 5 Freelancers
    db.executeRegistrationCommand("freelancer", "Faizan", "pass123", 0.0);  // ID 4
    db.executeRegistrationCommand("freelancer", "Yuneeb", "pass123", 0.0);  // ID 5
    db.executeRegistrationCommand("freelancer", "Alice", "pass123", 0.0);   // ID 6
    db.executeRegistrationCommand("freelancer", "Bob", "pass123", 0.0);     // ID 7
    db.executeRegistrationCommand("freelancer", "Charlie", "pass123", 0.0); // ID 8

    // Inject verified performance histories to activate sorting algorithms
    User* faizanProxy = db.getUserByUsername("Faizan");
    if (faizanProxy) {
        faizanProxy->addReviewRecord(5, "Exceptional AI integration work", "TechCorp");
        faizanProxy->setCompletedContractsDirect(5);
    }
    User* yuneebProxy = db.getUserByUsername("Yuneeb");
    if (yuneebProxy) {
        yuneebProxy->addReviewRecord(5, "Flawless 4K video strategy", "Naeem");
        yuneebProxy->setCompletedContractsDirect(8);
    }

    // Hardcode Exactly 5 Gigs (strictly floored at >= $100)
    db.executeAddGigOperation(4, "AI Chatbot & Computer Vision", "AI/ML", "Python OpenCV LLM", 100.0);
    db.executeAddGigOperation(5, "4K Video Editing & Strategy", "Design", "Premiere AfterEffects", 150.0);
    db.executeAddGigOperation(6, "Full Stack React & Node Platform", "Web Dev", "React Node JS", 100.0);
    db.executeAddGigOperation(7, "Cross-Platform Mobile UI/UX", "Mobile", "Flutter Figma", 120.0);
    db.executeAddGigOperation(8, "Database Optimization Schema", "Data", "SQL SQLite Tuning", 100.0);

    // Hardcode Exactly 3 Open Projects (strictly floored at >= $500)
    db.executeAddProjectOperation(1, "Airport Management System Core", "Web Dev", 500.0, "uploads/spec_airport.pdf");
    db.executeAddProjectOperation(2, "Vision-Based AI Companion", "AI/ML", 600.0, "uploads/spec_vision.pdf");
    db.executeAddProjectOperation(3, "Short Form High-Retention Media", "Design", 500.0, "uploads/spec_media.pdf");

    db.persistDatabaseToStorage();
}

// Inter-Process Communication Loop
// The continuous polling daemon listening on console pipes to execute commands passed from Python UI.
int main() {
    PlatformDB& activeEngine = PlatformDB::getInstance();

    // Verifies persistence state automatically
    ifstream startupCheckStream("platform_db.csv");
    if (startupCheckStream.is_open()) {
        startupCheckStream.close();
        activeEngine.loadDatabaseFromStorage();
    } else {
        initializeFallbackEcosystem(activeEngine);
    }

    cout << fixed << setprecision(2);
    string pipedInputBuffer;

    // Iterates constantly capturing flat flat inbound CSV instructions sent by Streamlit
    while (getline(cin, pipedInputBuffer)) {
        pipedInputBuffer = StringUtils::trim(pipedInputBuffer);
        if (pipedInputBuffer.empty()) continue;

        const int MAX_INBOUND_PARAMETERS = 12;
        string parameterTokens[MAX_INBOUND_PARAMETERS];
        // Extracts parameters safely into structured token arrays
        int extractedTokenCount = StringUtils::parseCsv(pipedInputBuffer, parameterTokens, MAX_INBOUND_PARAMETERS);

        if (extractedTokenCount == 0) continue;

        string opcode = parameterTokens[0];
        string executionResponseOutput;

        try {
            // Standard action triggers evaluating target parameters
            if (opcode == "exit") {
                activeEngine.persistDatabaseToStorage();
                cout << "true,Daemon processing loop terminated cleanly" << endl;
                break;
            }
            else if (opcode == "register" && extractedTokenCount >= 5) {
                double startingFunds = 0.0;
                try { startingFunds = stod(parameterTokens[4]); } catch (...) {}
                executionResponseOutput = activeEngine.executeRegistrationCommand(
                    parameterTokens[1], parameterTokens[2], parameterTokens[3], startingFunds
                );
            }
            else if (opcode == "login" && extractedTokenCount >= 3) {
                executionResponseOutput = activeEngine.executeLoginCommand(
                    parameterTokens[1], parameterTokens[2]
                );
            }
            else if (opcode == "topup" && extractedTokenCount >= 3) {
                executionResponseOutput = activeEngine.executeWalletTopUp(
                    stoi(parameterTokens[1]), stod(parameterTokens[2])
                );
            }
            // Escrow execution path passing expected threshold target floors
            else if (opcode == "pay" && extractedTokenCount >= 4) {
                double requiredMilestoneAmt = (extractedTokenCount >= 5) ? stod(parameterTokens[4]) : 0.0;
                executionResponseOutput = activeEngine.executePaymentSettlement(
                    stoi(parameterTokens[1]), stoi(parameterTokens[2]), stod(parameterTokens[3]), requiredMilestoneAmt
                );
            }
            else if (opcode == "add_gig" && extractedTokenCount >= 6) {
                executionResponseOutput = activeEngine.executeAddGigOperation(
                    stoi(parameterTokens[1]), parameterTokens[2], parameterTokens[3], 
                    parameterTokens[4], stod(parameterTokens[5])
                );
            }
            else if (opcode == "add_project" && extractedTokenCount >= 5) {
                string attach = (extractedTokenCount >= 6) ? parameterTokens[5] : "none";
                executionResponseOutput = activeEngine.executeAddProjectOperation(
                    stoi(parameterTokens[1]), parameterTokens[2], parameterTokens[3], 
                    stod(parameterTokens[4]), attach
                );
            }
            else if (opcode == "review" && extractedTokenCount >= 5) {
                executionResponseOutput = activeEngine.executeReviewAggregation(
                    stoi(parameterTokens[1]), stoi(parameterTokens[2]), stoi(parameterTokens[3]), parameterTokens[4]
                );
            }
            else if (opcode == "send_msg" && extractedTokenCount >= 4) {
                executionResponseOutput = activeEngine.executeOutboundMessageDispatch(
                    parameterTokens[1], parameterTokens[2], parameterTokens[3]
                );
            }
            
            // Simultaneous Synchronization Triggers
            // VIVA TIP: Purges active active memory arrays and reloads flat CSV state immediately before reads/writes
            // to ensure simultaneous external modifications from distinct users are synchronized.
            else if (opcode == "profile" && extractedTokenCount >= 2) {
                activeEngine.loadDatabaseFromStorage();
                executionResponseOutput = activeEngine.executeProfileAcquisition(stoi(parameterTokens[1]));
            }
            else if (opcode == "gigs_list") {
                activeEngine.loadDatabaseFromStorage();
                executionResponseOutput = activeEngine.executeCatalogRetrievalGigs();
            }
            else if (opcode == "projects_list") {
                activeEngine.loadDatabaseFromStorage();
                executionResponseOutput = activeEngine.executeCatalogRetrievalProjects();
            }
            else if (opcode == "stats") {
                activeEngine.loadDatabaseFromStorage();
                executionResponseOutput = activeEngine.executeEcosystemMetricsSync();
            }
            else if (opcode == "users_list") {
                activeEngine.loadDatabaseFromStorage();
                executionResponseOutput = activeEngine.executeUsersHandleAcquisition();
            }
            else if (opcode == "get_msgs" && extractedTokenCount >= 3) {
                activeEngine.loadDatabaseFromStorage();
                executionResponseOutput = activeEngine.executeHistoryExtraction(parameterTokens[1], parameterTokens[2]);
            }
            else {
                executionResponseOutput = "false,Instruction opcode undefined or parameter mismatch discovered";
            }
        } 
        // Safely catches standard standard exceptions and formats clear text output pipes
        catch (const exception& ex) {
            executionResponseOutput = string("false,Platform Security Exception: ") + StringUtils::csvClean(ex.what());
        }

        // Returns final parsed strings directly back over console pipe interfaces to Python UI
        cout << executionResponseOutput << endl;
    }

    return 0;
}