#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

struct Task {
    string id;
    string name;
    int priority;
    vector<string> depends_on;
    int duration_ms;
};

struct PQItem {
    int priority;
    string id;

    bool operator<(const PQItem &other) const {
        return priority > other.priority; // smaller number = higher priority
    }
};

class Scheduler {

private:
    unordered_map<string, Task> tasks;
    unordered_map<string, vector<string>> graph;
    unordered_map<string, int> indegree;

    priority_queue<PQItem> readyQueue;

    mutex mtx;
    condition_variable cv;

    int workers;
    int completed = 0;

    unordered_map<string, chrono::steady_clock::time_point> startTimes;
    unordered_map<string, chrono::steady_clock::time_point> endTimes;

public:

    Scheduler(int w) {
        workers = w;
    }

    void loadTasks(string filename) {

        ifstream file(filename);
        json data;
        file >> data;

        for (auto &t : data["tasks"]) {

            Task task;
            task.id = t["id"];
            task.name = t["name"];
            task.priority = t["priority"];
            task.depends_on = t["depends_on"].get<vector<string>>();
            task.duration_ms = t["duration_ms"];

            tasks[task.id] = task;

            indegree[task.id] = task.depends_on.size();

            for (auto &dep : task.depends_on) {
                graph[dep].push_back(task.id);
            }
        }
    }

    bool detectCycle() {

        unordered_map<string, int> temp = indegree;
        queue<string> q;

        for (auto &p : temp)
            if (p.second == 0)
                q.push(p.first);

        int count = 0;

        while (!q.empty()) {

            string cur = q.front();
            q.pop();

            count++;

            for (auto &next : graph[cur]) {
                if (--temp[next] == 0)
                    q.push(next);
            }
        }

        return count != tasks.size();
    }

    void initializeQueue() {

        for (auto &p : indegree) {

            if (p.second == 0) {

                readyQueue.push({
                    tasks[p.first].priority,
                    p.first
                });

            }
        }
    }

    void workerThread() {

        while (true) {

            unique_lock<mutex> lock(mtx);

            cv.wait(lock, [&] {
                return !readyQueue.empty() || completed == tasks.size();
            });

            if (completed == tasks.size())
                return;

            PQItem item = readyQueue.top();
            readyQueue.pop();

            Task task = tasks[item.id];

            startTimes[task.id] = chrono::steady_clock::now();

            cout << "START " << task.id << endl;

            lock.unlock();

            this_thread::sleep_for(
                chrono::milliseconds(task.duration_ms)
            );

            lock.lock();

            cout << "END " << task.id << endl;

            endTimes[task.id] = chrono::steady_clock::now();

            completed++;

            for (auto &next : graph[task.id]) {

                if (--indegree[next] == 0) {

                    readyQueue.push({
                        tasks[next].priority,
                        next
                    });

                }
            }

            cv.notify_all();
        }
    }

    void run() {

        initializeQueue();

        auto start = chrono::steady_clock::now();

        vector<thread> pool;

        for (int i = 0; i < workers; i++) {
            pool.emplace_back(&Scheduler::workerThread, this);
        }

        cv.notify_all();

        for (auto &t : pool)
            t.join();

        auto end = chrono::steady_clock::now();

        cout << "\n----- FINAL REPORT -----\n";

        auto total = chrono::duration_cast<
            chrono::milliseconds
        >(end - start);

        cout << "Total Time: "
             << total.count()
             << " ms\n";

        cout << "\nTask Timings:\n";

        for (auto &p : tasks) {

            auto st = chrono::duration_cast<
                chrono::milliseconds
            >(startTimes[p.first] - start);

            auto et = chrono::duration_cast<
                chrono::milliseconds
            >(endTimes[p.first] - start);

            cout << p.first
                 << " start=" << st.count()
                 << "ms end=" << et.count()
                 << "ms\n";
        }
    }
};

int main(int argc, char* argv[]) {

    int workers = 4;

    if (argc == 3 && string(argv[1]) == "--workers")
        workers = stoi(argv[2]);

    Scheduler scheduler(workers);

    scheduler.loadTasks("tasks.json");

    if (scheduler.detectCycle()) {

        cout << "ERROR: Circular dependency detected\n";
        return 1;

    }

    scheduler.run();

}