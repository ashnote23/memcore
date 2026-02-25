#pragma once

#include "model.h"
#include <vector> 
#include <unordered_map> 


class Scheduler {
private:
    std::unordered_map<UserId, User> users;
public:
    Scheduler() = default;

    void create_user(UserId uid);

    bool user_exists(UserId uid) const;

    const std::unordered_map<UserId, User>& get_users() const;

    bool card_exists(UserId uid, CardId cid) const;

    bool topic_exists(UserId uid, TopicId tid) const;

    void create_topic(UserId uid, const Topic& topic);

    void add_card(UserId uid, const Card& card);

    void review_complete(UserId uid, CardId cardId, int rating);

    std::vector<CardId> get_due_cards(UserId uid, Date date, TopicId topicId = -1);
};