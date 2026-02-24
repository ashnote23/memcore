#include <iostream>
#include "scheduler.h"

int main() {

    //Create Scheduler
    Scheduler scheduler;

    //Define User and Topic
    UserId uid = 1;
    TopicId topicId = 100;

    //Create Topic
    Topic topic;
    topic.id = topicId;
    topic.name = "Sample topic";

    scheduler.create_topic(uid, topic);

    //Create Cards
    // Card card1(1, topicId);
    // Card card2(2, topicId);
    Card card1, card2;
    card1.id = 1;
    card1.topicId = topicId;

    card2.id = 2;
    card2.topicId = topicId;

    scheduler.add_card(uid, card1);
    scheduler.add_card(uid, card2);

    //Simulate Days
    Date currentDay = 0;

    while (currentDay <= 20) {

        std::cout << "Day: " << currentDay << "\n";

        //Get Due Cards
        std::vector<CardId> dueCards =
            scheduler.get_due_cards(uid, currentDay);

        std::cout<<"Due count: " << dueCards.size() << "\n";


        //Process Due Cards
        for (CardId id : dueCards) {

            std::cout << "Reviewing Card: " << id << "\n";

            int rating;

            // TODO: Decide rating logic (hardcoded or input)
            rating = 3;  

            scheduler.review_complete(uid, id, rating);
        }

        std::cout << "-----------------------\n";

        //Move to Next Day
        currentDay++;
    }

    return 0;
}
