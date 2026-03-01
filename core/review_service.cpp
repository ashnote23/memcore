#include "review_service.h"
#include<stdexcept>

using namespace std;

void ReviewService::create_user(UserId uid)
{
    scheduler.create_user(uid);
}

bool ReviewService::user_exists(UserId uid)
{
    return scheduler.user_exists(uid);
}

Card ReviewService::get_card(UserId uid, CardId cid)
{
    const auto& users = scheduler.get_users();
    return users.at(uid).cards.at(cid);
}

void ReviewService::create_topic(UserId uid, TopicId tid, const std::string& name)
{
    Topic topic;
    topic.id = tid;
    topic.name = name;
    scheduler.create_topic(uid, topic);
}

void ReviewService::add_card(UserId uid, CardId cid, TopicId tid)
{
    Card card;
    card.id = cid;
    card.topicId = tid;
    scheduler.add_card(uid, card);
}

vector<CardId> ReviewService::get_due_cards(UserId uid, Date date, TopicId tid)
{
    if(user_exists(uid))
        return scheduler.get_due_cards(uid, date, tid);
    return {};
}

void ReviewService::review_card(UserId uid, CardId cid, int rating)
{
    if (rating < 0 || rating > 3) {
        throw std::invalid_argument("Rating must be between 0 and 3");
    }

    if(user_exists(uid) && scheduler.card_exists(uid, cid))
        scheduler.review_complete(uid, cid, rating);
}