#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <limits>
#include <algorithm>

using namespace std;

typedef pair<int, int> pii;

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

// run the Bellman-Ford algorithm and find shortest paths 
map<pii, DistanceInfo> bellmanFord(const vector<vector<pii>>& graph, int src, int V) {
    map<pii, DistanceInfo> distances;
    // initialize distances from source to all nodes as infinite
    //except itself (0)
    for (int i = 0; i < V; i++) {
        distances[{src, i}] = {numeric_limits<int>::max(), {}};
    }
    distances[{src, src}] = {0, {src}};

    // edges up to V-1 times
    for (int i = 1; i < V; i++) {
        for (int j = 0; j < V; j++) {
            for (const auto& edge : graph[j]) {
                int from = j, to = edge.first, weight = edge.second;
                if (distances[{src, from}].cost != numeric_limits<int>::max() &&
                    distances[{src, to}].cost > distances[{src, from}].cost + weight) {
                    distances[{src, to}].cost = distances[{src, from}].cost + weight;
                    distances[{src, to}].path = distances[{src, from}].path;
                    distances[{src, to}].path.push_back(to);
                }
            }
        }
    }
    return distances;
}

//  process messages and write the results to the output file
void runAndWriteToFile(vector<vector<pii>>& graph, int maxNodeNum, const set<int>& nodesInGraph, 
                       const vector<Message>& messages, ofstream& outputFile) {
    map<pii, DistanceInfo> shortestPathMap;
    // compute shortest paths for each node 
    for (int i : nodesInGraph) {
        map<pii, DistanceInfo> oneNodeMap = bellmanFord(graph, i, maxNodeNum);
        shortestPathMap.insert(oneNodeMap.begin(), oneNodeMap.end());
    }

    // write messages and their paths to the output file
    for (const auto& msg : messages) {
        const auto& pathInfo = shortestPathMap[{msg.src, msg.dest}];
        outputFile << "from " << msg.src << " to " << msg.dest;
        if (pathInfo.cost == numeric_limits<int>::max()) {
            outputFile << " cost infinite hops unreachable message " << msg.content << endl;
        } else {
            outputFile << " cost " << pathInfo.cost << " hops ";
            for (int hop : pathInfo.path) {
                outputFile << hop << " ";
            }
            outputFile << "message " << msg.content << endl;
        }
    }
}

// read the graph from a file
vector<vector<pii>> readGraph(const char* filename, int& maxNodeNum, set<int>& nodesInGraph) {
    ifstream file(filename);
    vector<vector<pii>> graph(maxNodeNum);
    int src, dest, cost;
    // read each line of the file and update the graph, node count, node set
    while (file >> src >> dest >> cost) {
        graph[src].push_back({dest, cost});
        graph[dest].push_back({src, cost});
        nodesInGraph.insert(src);
        nodesInGraph.insert(dest);
        maxNodeNum = max(maxNodeNum, max(src, dest) + 1);
    }
    return graph;
}

// read messages from a file
vector<Message> readMessages(const char* filename) {
    vector<Message> messages;
    ifstream file(filename);
    int src, dest;
    string content;
    // read each line for source, destination and message content
    while (file >> src >> dest) {
        getline(file, content);
        messages.push_back({src, dest, content.substr(1)}); 
    }
    return messages;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        cerr << "Usage: ./distvec topofile messagefile changesfile\n";
        return 1;
    }

    int maxNodeNum = 0;
    set<int> nodesInGraph;
    vector<vector<pii>> graph = readGraph(argv[1], maxNodeNum, nodesInGraph);
    vector<Message> messages = readMessages(argv[2]);

    ofstream outputFile("output.txt");
    runAndWriteToFile(graph, maxNodeNum, nodesInGraph, messages, outputFile);
    outputFile.close();
    return 0;
}

