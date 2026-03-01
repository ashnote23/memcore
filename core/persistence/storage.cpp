#include "storage.h"
#include <fstream>
#include <cstdint>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

using namespace std;

struct LogRecord {
    UserId userId;    // offset 0
    CardId cardId;    // offset 4
    int rating;       // offset 8
    int timestamp;    // offset 12
    uint32_t crc;     // offset 16
};

uint32_t crc32(const void* data, size_t length) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

void Storage::save_snapshot(const std::unordered_map<UserId, User>& users)
{
    int total = 0;
    for(const auto& [id, user] : users)
    {   
        total+=user.cards.size();
    }
    std::ofstream snapShot(snapshot_path, std::ios::binary);
    if (!snapShot.is_open())
        return;
    //writing the total number of cards at the start of the snapshot 
    snapShot.write(reinterpret_cast<const char*>(&total), sizeof(total));

    // Second loop - write card data
    for (const auto& [id, user] : users)
    {
        for (const auto& [cardId, card] : user.cards)
        {
            // Write userId
            snapShot.write(reinterpret_cast<const char*>(&user.id), sizeof(user.id));

            // Write card.id
            snapShot.write(reinterpret_cast<const char*>(&card.id), sizeof(card.id));

            // Write card.topicId
            snapShot.write(reinterpret_cast<const char*>(&card.topicId), sizeof(card.topicId));

            // Write card.easeFactor
            snapShot.write(reinterpret_cast<const char*>(&card.easeFactor), sizeof(card.easeFactor));

            // Write card.interval
            snapShot.write(reinterpret_cast<const char*>(&card.interval), sizeof(card.interval));

            // Write card.repetitions
            snapShot.write(reinterpret_cast<const char*>(&card.repetitions), sizeof(card.repetitions));

            // Write card.nextReviewDate
            snapShot.write(reinterpret_cast<const char*>(&card.nextReviewDate), sizeof(card.nextReviewDate));
        }
    }
    // clear the log after snapshot
    std::ofstream clearLog(log_path, std::ios::trunc);
}

void Storage::load_snapshot(Scheduler& scheduler)
{
    std::ifstream file(snapshot_path, std::ios::binary);
    if(!file.is_open())
        return;
    
    int total;
    Card card;
    User user;
    file.read(reinterpret_cast<char*>(&total), sizeof(total));

    for (int i = 0; i < total; i++) {
        // read each field in exact same order as write
        file.read(reinterpret_cast<char*>(&user.id), sizeof(user.id));
        file.read(reinterpret_cast<char*>(&card.id), sizeof(card.id));
        file.read(reinterpret_cast<char*>(&card.topicId), sizeof(card.topicId));
        file.read(reinterpret_cast<char*>(&card.easeFactor), sizeof(card.easeFactor));
        file.read(reinterpret_cast<char*>(&card.interval), sizeof(card.interval));
        file.read(reinterpret_cast<char*>(&card.repetitions), sizeof(card.repetitions));
        file.read(reinterpret_cast<char*>(&card.nextReviewDate), sizeof(card.nextReviewDate));
        
        scheduler.create_user(user.id);//this would if the user already exists
        scheduler.add_card(user.id, card);
    }
}

void Storage::append_log(UserId uid, CardId cid, int rating, int timestamp)
{
    std::ofstream logRecord(log_path, std::ios::app | std::ios::binary);
    if(!logRecord.is_open())
        return;

    LogRecord record;
    record.userId = uid;
    record.cardId = cid;
    record.rating = rating;
    record.timestamp = timestamp;
    record.crc = crc32(&record, sizeof(LogRecord) - sizeof(uint32_t));
    logRecord.write(reinterpret_cast<const char*>(&record), sizeof(record));
    logRecord.flush();
}

void Storage::replay_log(Scheduler& scheduler)
{
    std::ifstream file(log_path, std::ios::binary);
    if(!file.is_open())
        return;
    LogRecord record;
    while(file.read(reinterpret_cast<char*>(&record), sizeof(record))) {
        uint32_t expected = crc32(&record, sizeof(LogRecord) - sizeof(uint32_t));
    if (expected != record.crc)
        break;
    else    
        scheduler.review_complete(record.userId, record.cardId, record.rating);
    }
}

