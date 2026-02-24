#include <iostream>
#include <queue> // Required header for priority_queue
#include <vector> // Required for min-heap implementation
#include <unordered_map>
#include <string>


using UserId = int;
using CardId = int;
using TopicId = int;
using Date   = int;

struct Card {
    CardId id;
    TopicId topicId;

    double easeFactor = 1.3;
    int interval = 0;
    int repetitions = 0;

    Date nextReviewDate = 0;
};

struct Topic {
    TopicId id;
    std::string name;

    //  Topic(TopicId tid, const std::string& tname)
    //     : id(tid), name(tname) {}
};

struct CompareByDueDate {
    const std::unordered_map<CardId, Card>* cards;

    CompareByDueDate(const std::unordered_map<CardId, Card>* c)
        : cards(c) {}

    bool operator()(CardId a, CardId b) const {
        return cards->at(a).nextReviewDate >
               cards->at(b).nextReviewDate;
    }
};

struct User {
    UserId id;

    std::unordered_map<CardId, Card> cards;
    std::unordered_map<TopicId, Topic> topics;

    std::priority_queue<
        std::pair<Date, CardId>,
        std::vector<std::pair<Date, CardId>>,
        std::greater<>
    > dueQueue;

    User() = default;
    User(UserId uid)
        : id(uid) {}
};