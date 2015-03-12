#include <mpi.h>
#include "bfs.hpp"
#include "LineReader.hpp"
#include <sys/stat.h>
#include <stdio.h>
#include <vector>
#include <string.h>
#include <string>
#include <sstream>
#include <functional>

#include <fstream>
using namespace std;

#define LENG 0
#define DATA 1
#define RESPONSE 2

const int LOW = 1;
const int HIGH = 2;
const int NO_DATA = 3;
const int HAVE_DATA = 4;
const int MORE_DATA = 5;

vector<string> split(const char* strChar, char delimiter) {
    vector<string> internal;
    stringstream ss(strChar); // Turn the string into a stream.
    string tok;

    if (delimiter == ' ' || delimiter == '\t') {
        while (ss) {
            ss >> tok;
            internal.push_back(tok);
        }
    } else {
        while (getline(ss, tok, delimiter)) {
            internal.push_back(tok);
        }
    }
    return internal;
}

int hashKey(string data, int n) {
    hash<string> str_hash;
    size_t value = str_hash(data);
    return value % n;
}

void storeNewNode(int size, string key, string data, vector<string>* &otherdata) {
    int id = hashKey(key, size);
    otherdata[id].push_back(data);    
}

vector<string> parseNode(const char* str) {
    vector<string> result;
    vector<string> internal = split(str, ' ');
    result.push_back(internal[0]);
    vector<string> edges = split(internal[1].c_str(), '|');
    result.insert(result.end(), edges.begin(), edges.end());
    return result;
}

void createNode(string& result, string id, string nodes, string dist, string color) {
    result.clear();
    result.append(id);
    result.append("\t");
    result.append(nodes);
    result.append("|");
    result.append(dist);
    result.append("|");
    result.append(color);
    result.append("|");
}

void increaseDist(string& output, string distance) {
    if (distance.compare("Integer.MAX_VALUE") == 0) {
        output = distance;
        return;
    }

    int value = stoi(distance);
    output = to_string(value + 1);
}

void minDist(string& output, string dist1, string dist2) {
    if (dist1 == "Integer.MAX_VALUE") {
        output = dist2;
        return;
    } else if (dist2 == "Integer.MAX_VALUE") {
        output = dist1;
        return;
    }

    int value1 = stoi(dist1);
    int value2 = stoi(dist2);
    output = value1 < value2 ? dist1 : dist2;
}

int encodeColor(string color) {
    if (color == "WHITE")
        return 0;
    if (color == "GRAY")
        return 1;
    if (color == "BLACK")
        return 2;
    return -1;
}

void maxColor(string& output, string color1, string color2) {
    int c1 = encodeColor(color1);
    int c2 = encodeColor(color2);
    output = c1 < c2 ? color2 : color1;
}

void swap(string& s1, string& s2) {
    string temp;
    temp = s1;
    s1 = s2;
    s2 = temp;
}

void siftDown(vector<string>& data, int start, int n) {
    while (start * 2 + 1 < n) {
        // children are 2*i + 1 and 2*i + 2
        int child = start * 2 + 1;
        // get bigger child
        if (child + 1 < n && data[child] < data[child + 1]) child++;

        if (data[start] < data[child]) {
            swap(data[start], data[child]);
            start = child;
        } else
            return;
    }
}

void sort(vector<string> &data) { // heap-sort
    int size = data.size();
    if (size == 0)
        return;
    for (int k = size / 2; k >= 0; k--) {
        siftDown(data, k, size);
    }

    while (size > 1) {
        swap(data[size - 1], data[0]);
        siftDown(data, 0, size - 1);
        size--;
    }
}

void sendData(int target, vector<string> &data, int count) {
    MPI_Send(&count, 1, MPI_INT, target, LENG, MPI_COMM_WORLD);
    for (int i = 0; i < count; i++) {
        int data_len = data[i].length();
        MPI_Send(&data_len, 1, MPI_INT, target, LENG, MPI_COMM_WORLD);
        MPI_Send(data[i].c_str(), data_len, MPI_CHAR, target, DATA, MPI_COMM_WORLD);
    }
}

void receive_resp(int from, vector<string> &output) {
    int len;
    MPI_Recv(&len, 1, MPI_INT, from, LENG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    for (int i = 0; i < len; i++) {
        int data_len;
        MPI_Recv(&data_len, 1, MPI_INT, from, LENG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        char* rdata = new char[data_len + 1];
        rdata[data_len] = '\0';
        MPI_Recv(rdata, data_len, MPI_CHAR, from, DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        string d(rdata);
        output.push_back(d);

        delete []rdata;
    }
}

void reduce(vector<string> &input, vector<string> &output) {
    string previous("NULL");
    string edges("NULL");
    string distance("Integer.MAX_VALUE");
    string color("WHITE");

    sort(input);
    for (int k = 0; k < input.size(); k++) {
        vector<string> node = parseNode(input[k].c_str());
        string node_id = node[0];
        if (node_id.compare(previous) != 0) {
            if (k > 0) {
                string nnode;
                createNode(nnode, previous, edges, distance, color);
                output.push_back(nnode);
            }
            previous = node_id;
            edges = "NULL";
            distance = "Integer.MAX_VALUE";
            color = "WHITE";
        }

        if (node[1].compare("NULL") != 0) {
            edges = node[1];
        }
        minDist(distance, distance, node[2]);
        maxColor(color, color, node[3]);
    }

    if (input.size() > 0) {
        string nnode;
        createNode(nnode, previous, edges, distance, color);
        output.push_back(nnode);
    }
    //printf("reduce from %ld to %ld\n", input.size(), output.size());
}

int BFS::run(int argc, char* argv[]) {
    int size, rank;
    MPI::Init(argc, argv);
    size = MPI::COMM_WORLD.Get_size();
    rank = MPI::COMM_WORLD.Get_rank();

    const int FINISH = -1;

    MPI_Barrier(MPI_COMM_WORLD);
    double startTime = MPI::Wtime();
    double transmit_time, elapsed, local_elapsed;

    // Read data and distribute
    vector<string> input;

    if (rank == 0) {
        FILE* file;
        size_t total_size;
        const char* name = "graph.txt";
        LineReader* reader;
        
        if (argc >= 3)
            name = argv[2];
        
        file = fopen(name, "r+");

        struct stat filestatus;
        stat(name, &filestatus);
        total_size = filestatus.st_size;

        reader = new LineReader(file, total_size);
        char* line = NULL;
        reader->readLine(line);
        
        int id = 0;
        while (line != NULL) {
            if (id == 0) {
                string node(line);
                input.push_back(node);
            }
            else { // send this data to worker process
                int len = strlen(line);
                MPI_Send(&len, 1, MPI_INT, id, LENG, MPI_COMM_WORLD);
                MPI_Send(line, strlen(line), MPI_CHAR, id, DATA, MPI_COMM_WORLD);
            }
            
            delete []line;
            reader->readLine(line);

            id = (id + 1) % size;
        }
        for (int i = 1; i < size; i++) {
            MPI_Send((void *) &FINISH, 1, MPI_INT, i, LENG, MPI_COMM_WORLD);
        }

        fclose(file);
        reader->close();
        delete reader;
        
        printf("Finish reading data\n");
    } else { // receive data from server
        int len;
        while (true) {
            MPI_Recv(&len, 1, MPI_INT, 0, LENG, MPI_COMM_WORLD, MPI_STATUSES_IGNORE);
            if (len == FINISH) break;

            char* line = new char[len + 1];
            line[len] = '\0';
            MPI_Recv(line, len, MPI_CHAR, 0, DATA, MPI_COMM_WORLD, MPI_STATUSES_IGNORE);
            string node(line);
            input.push_back(node);
            delete []line;
        }
    }

    double transmit_finish = MPI::Wtime() - startTime;
    MPI_Reduce(&transmit_finish, &transmit_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    // Process: map data, sort, exchange, merge

    bool newLoop = true;

    int m = 0;

    vector<string>* other_data_tmp = new vector<string>[size];
    vector<string>* other_data = new vector<string>[size];
    while (newLoop) {
        m++;
        if (rank == 0)
            printf("Iteration %d\n", m);
        newLoop = false;
        // Step 1: map data
        for (int i = 0; i < size - 1; i++)
            other_data[i].clear();
        int pack = 0;
        for (int i = 0; i < input.size(); i++) { 
            vector<string> node = parseNode(input[i].c_str());
            if (node[3].compare("GRAY") == 0) {
                vector<string> edges = split(node[1].c_str(), ',');
                for (int i = 0; i < edges.size(); i++) {
                    string new_node;
                    string newDist;
                    increaseDist(newDist, node[2]);
                    createNode(new_node, edges[i], "NULL", newDist, "GRAY");
                    storeNewNode(size, edges[i], new_node, other_data_tmp);
                }
                string oldNode;
                createNode(oldNode, node[0], node[1], node[2], "BLACK");
                storeNewNode(size, node[0], oldNode, other_data_tmp);
                pack += edges.size();
            } else
                storeNewNode(size, node[0], input[i], other_data_tmp);
            pack++;
            if (pack >= 1500000) {
                pack = 0;
                for (int i = 0; i < size; i++) {
                    other_data_tmp[i].insert(other_data_tmp[i].end(), other_data[i].begin(), other_data[i].end());
                    other_data[i].clear();
                    reduce(other_data_tmp[i], other_data[i]);
                    other_data_tmp[i].clear();
                }
            }
        }

        printf("Step 2: send data to corresponding processes...\n");
        for (int i = 0; i < size; i++) {
            reduce(other_data_tmp[i], other_data[i]);
            other_data_tmp[i].clear();
        }

        // for each phase, 1 process sends, others receive.
        for (int phase = 0; phase < size; phase++) {
            if (phase == rank) {// send
                for (int i = 0; i < size; i++) {
                    if (i == rank)
                        continue;
                    sendData(i, other_data[i], other_data[i].size());
                    other_data[i].clear();
                }
            } else {// receive
                receive_resp(phase, other_data[rank]);
            }
        }
        

        // Step 3: Sort data
        printf("Step 3: Process %d is sorting %ld nodes...\n", rank, other_data[rank].size());
        sort(other_data[rank]);
      
        // reduce phase: find the smallest distance
        printf("Step 4: Process %d is running reduce: %ld nodes...\n", rank, other_data[rank].size());
        input.clear();

        string previous("NULL");
        string edges("NULL");
        string distance("Integer.MAX_VALUE");
        string color("WHITE");
        bool hasGray = false;
        for (int k = 0; k < other_data[rank].size(); k++) {
            vector<string> node = parseNode(other_data[rank][k].c_str());
            string node_id = node[0];
            
            if (node_id.compare(previous) != 0) {
                if (k > 0) {
                    string nnode;
                    createNode(nnode, previous, edges, distance, color);
                    input.push_back(nnode);
                    
                    if (color.compare("GRAY") == 0)
                        hasGray = true;
                }
                previous = node_id;
                edges = "NULL";
                distance = "Integer.MAX_VALUE";
                color = "WHITE";
            }

            if (node[1].compare("NULL") != 0) {
                edges = node[1];
            }
            minDist(distance, distance, node[2]);
            maxColor(color, color, node[3]);
        }
        if (other_data[rank].size() > 0) {
            string nnode;
            createNode(nnode, previous, edges, distance, color);
            input.push_back(nnode);
        }

        other_data[rank].clear();
        
        if (color.compare("GRAY") == 0)
            hasGray = true;

        MPI_Allreduce(&hasGray, &newLoop, 1, MPI_BYTE, MPI_BOR, MPI_COMM_WORLD);
    }

    string out_name("graph");
    out_name.append(to_string(rank));
    out_name.append(".txt");
    ofstream out(out_name);


    for (int k = 0; k < input.size(); k++) {
        out << input[k] << "\n";
    }

    out.close();
    input.clear();
    delete []other_data_tmp;
    delete []other_data;
    local_elapsed = MPI::Wtime() - startTime;
    MPI_Reduce(&local_elapsed, &elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        cout << "Read and distribution time: " << transmit_time << " seconds\n";
        cout << "Total time: " << elapsed << " seconds\n";
    }

    MPI::Finalize();
    return 0;
}

