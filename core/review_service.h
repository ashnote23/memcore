#pragma once
#include "scheduler.h"

class ReviewService {
private:
    Scheduler& scheduler;

public:
    ReviewService(Scheduler& schedulerRef) 
        : scheduler(schedulerRef) {};

    void create_user(UserId uid);

    bool user_exists(UserId uid);

    void create_topic(UserId uid, TopicId tid, const std::string& name);

    void add_card(UserId uid, CardId cid, TopicId tid);

    std::vector<CardId> get_due_cards(UserId uid, Date date, TopicId topicId=-1);

    void review_card(UserId uid, CardId cid, int rating);
};
