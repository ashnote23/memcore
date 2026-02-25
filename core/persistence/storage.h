#pragma once
#include "../scheduler.h"
#include <string>
#include<unordered_map>

class Storage {
private:
    std::string log_path;
    std::string snapshot_path;
public:
    Storage(std::string logPath, std::string snapshotPath)
        : log_path(logPath), snapshot_path(snapshotPath) {};

    void save_snapshot(const std::unordered_map<UserId, User>& user);

    void load_snapshot(Scheduler& scheduler);

    void append_log(UserId uid, CardId cid, int rating, int timestamp);

    void replay_log(Scheduler& scheduler);
    
    void clear_log();
};