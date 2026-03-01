#include <iostream>
#include "core/scheduler.h"
#include "core/persistence/storage.h"
#include "core/review_service.h"
#include <filesystem>
#include <ctime>
#include "core/grpc_server.cpp"
#include <grpcpp/grpcpp.h>
  
int main() {

    //Create Scheduler
    Scheduler scheduler;

    ReviewService reviewService(scheduler);

    Storage storage("review.log", "snapshot.bin");

    UserId uid = 1;
    TopicId topicId = 100;
    if(!std::filesystem::exists("snapshot.bin"))
    {
         // //Define User and Topic

        //Create Topic
        Topic topic;
        topic.id = topicId;
        topic.name = "Sample topic";

        reviewService.create_topic(uid, topic.id, topic.name);

        //Create Cards
        // Card card1(1, topicId);
        // Card card2(2, topicId);
        Card card1, card2;
        card1.id = 1;
        card1.topicId = topicId;

        card2.id = 2;
        card2.topicId = topicId;

        reviewService.add_card(uid, card1.id, card1.topicId);
        reviewService.add_card(uid, card2.id, card2.topicId);

    }
    else {
        storage.load_snapshot(scheduler);

        storage.replay_log(scheduler);
    }

        Date currentDay = 0;

        while (currentDay <= 20) {

            std::cout << "Day: " << currentDay << "\n";

            //Get Due Cards
            std::vector<CardId> dueCards =
                reviewService.get_due_cards(uid, currentDay);

            std::cout<<"Due count: " << dueCards.size() << "\n";


            //Process Due Cards
            for (CardId id : dueCards) {

                std::cout << "Reviewing Card: " << id << "\n";

                int rating;

                // TODO: Decide rating logic (hardcoded or input)
                rating = 3;  

                reviewService.review_card(uid, id, rating);
                int timestamp = static_cast<int>(std::time(nullptr));
                storage.append_log(uid, id, rating, timestamp);
            }

            std::cout << "-----------------------\n";

            //Move to Next Day
            currentDay++;
        }
        storage.save_snapshot(scheduler.get_users());
        // Start gRPC server
        grpc::ServerBuilder builder;
        builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());

        FlashcardServiceImpl service(reviewService);
        builder.RegisterService(&service);

        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        std::cout << "gRPC server listening on port 50051\n";
        server->Wait();
    return 0;
}
