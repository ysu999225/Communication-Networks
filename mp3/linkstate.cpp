#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <queue>
#include <map>
#include <limits>
#include <cstring>
#include <algorithm>

using namespace std;

typedef pair<int, int> pii;
typedef vector<pii> vpii;

struct Message {
    int src, dest;
    string content;
};

struct Change {
    int src, dest, newCost;
};

struct DistanceInfo {
    int cost;
    vector<int> path;
};

// compare for priority queue used in Dijkstra's algorithm
struct CompareDistanceInfo {
    bool operator()(const pair<DistanceInfo, int>& a, const pair<DistanceInfo, int>& b) {
        return a.first.cost > b.first.cost;
    }
};


// remove edges from a graph
struct EraseIfFirstEqual {
    int value;
    EraseIfFirstEqual(int val) : value(val) {}
    bool operator()(const pii &p) const {
        return p.first == value;
    }
};


// Function to run Dijkstra's algorithm from a given source node
map<pii, DistanceInfo> dijkstra(const vector<vpii>& graph, int src) {
    int V = graph.size();
    map<pii, DistanceInfo> shortestPaths;
    priority_queue<pair<DistanceInfo, int>, vector<pair<DistanceInfo, int>>, CompareDistanceInfo> pq;
    vector<DistanceInfo> dist(V, {numeric_limits<int>::max(), {}});
    
    dist[src].cost = 0;
    dist[src].path.push_back(src);
    pq.push({dist[src], src});

    vector<bool> visited(V, false);

    while (!pq.empty()) {
        int u = pq.top().second;
        pq.pop();
        if (visited[u]) continue;
        visited[u] = true;

        for (auto& edge : graph[u]) {
            int v = edge.first, weight = edge.second;
            if (dist[u].cost + weight < dist[v].cost) {
                dist[v].cost = dist[u].cost + weight;
                dist[v].path = dist[u].path;
                dist[v].path.push_back(v);
                pq.push({dist[v], v});
            }
        }
    }

    for (int i = 0; i < V; i++) {
        if (dist[i].cost != numeric_limits<int>::max()) {
            shortestPaths[{src, i}] = dist[i];
        }
    }
    return shortestPaths;
}

// process changes in the graph and recompute shortest paths
void processChanges(vector<vpii>& graph, const vector<Change>& changes) {
    for (const auto& change : changes) {
        if (change.newCost == -999) {
            graph[change.src].erase(remove_if(graph[change.src].begin(), graph[change.src].end(), EraseIfFirstEqual(change.dest)), graph[change.src].end());
            graph[change.dest].erase(remove_if(graph[change.dest].begin(), graph[change.dest].end(), EraseIfFirstEqual(change.src)), graph[change.dest].end());
        } else {
            graph[change.src].push_back({change.dest, change.newCost});
            graph[change.dest].push_back({change.src, change.newCost});
        }
    }
}



void loadGraph(vector<vpii>& graph, const string& filename, int& maxNodes) {
    ifstream inFile(filename);
    if (!inFile.is_open()) {
        cerr << "Error opening file: " << filename << endl;
        exit(1);
    }

    int src, dest, cost;
    while (inFile >> src >> dest >> cost) {
        maxNodes = max(maxNodes, max(src, dest));
        graph[src].push_back({dest, cost});
        graph[dest].push_back({src, cost});
    }
    graph.resize(maxNodes + 1);
    inFile.close();
}



int main(int argc, char** argv) {
    if (argc != 4) {
        cerr << "Usage: ./linkstate topofile messagefile changesfile\n";
        return 1;
    }

    int maxNodes = 0;
    vector<vpii> graph;
    loadGraph(graph, argv[1], maxNodes);

    vector<Message> messages;
    ifstream messagefile(argv[2]);
    if (!messagefile.is_open()) {
        cerr << "Error opening message file.\n";
        return 1;
    }

    string content;
    int src, dest;
    while (messagefile >> src >> dest) {
        getline(messagefile, content);
        messages.push_back({src, dest, content.substr(1)});
    }
    messagefile.close();

    ifstream changesfile(argv[3]);
    vector<Change> changes;
    if (!changesfile.is_open()) {
        cerr << "Error opening changes file.\n";
        return 1;
    }
    int newCost;
    while (changesfile >> src >> dest >> newCost) {
        changes.push_back({src, dest, newCost});
    }
    changesfile.close();

    map<pii, DistanceInfo> shortestPathMap;
    for (int i = 0; i <= maxNodes; ++i) {
        auto paths = dijkstra(graph, i);
        shortestPathMap.insert(paths.begin(), paths.end());
    }

    ofstream outputFile("output.txt");
    for (const auto& msg : messages) {
        pii key = {msg.src, msg.dest};
        if (shortestPathMap.find(key) != shortestPathMap.end() && shortestPathMap[key].cost != numeric_limits<int>::max()) {
            outputFile << "from " << msg.src << " to " << msg.dest << " cost " << shortestPathMap[key].cost << " path: ";
            for (int p : shortestPathMap[key].path) outputFile << p << " ";
            outputFile << "message: " << msg.content << "\n";
        } else {
            outputFile << "from " << msg.src << " to " << msg.dest << " unreachable message: " << msg.content << "\n";
        }
    }
    processChanges(graph, changes);
    shortestPathMap.clear();
    for (int i = 0; i <= maxNodes; ++i) {
        auto paths = dijkstra(graph, i);
        shortestPathMap.insert(paths.begin(), paths.end());
    }

    return 0;
}
