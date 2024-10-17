//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef UTIL_H_
#define UTIL_H_

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <queue>
#include <stack>

using namespace std;

namespace util {
/*
 * Template function to print vectors of any type
 */
template <typename T>
void printVector(const vector<T>& vec) {
    for (const T &element : vec) {
        // Print each element followed by a space
        cout << element << "  ";
    }
    // Newline after printing the vector
    cout << endl;
}

/*
 * Template function to print a set of any type
 */
template <typename T>
void printSet(const set<T>& s) {
    for (const T& element : s) {
        cout << element << "  ";
    }
    cout << endl;
}

/*
 * Template function to print maps of any key-value types
 */
template <typename K, typename V>
void printMap(const map<K, V>& m) {
    for (const auto& pair : m) {
        cout << pair.first << ": " << pair.second << "  ";
    }
    cout << endl;
}

/*
 * Template function to print a stack of any type
 */
template <typename T>
void printStack(stack<T> s) {
    // Loop through the stack until it's empty
    while (!s.empty()) {
        cout << s.top() << "  ";
        s.pop();
    }
    cout << endl;
}

/*
 * Template function to print a queue of any type
 */
template <typename T>
void printQueue(queue<T> q) {
    // Loop through the queue until it's empty
    while (!q.empty()) {
        cout << q.front() << "  ";
        q.pop();
    }
    cout << endl;
}

/*
 * Template function to print a container of vector, set, or their variants
 */
template <typename T>
void printContainer(const T& container) {
    for (const auto& element : container) {
        cout << element << "  ";
    }
}

/*
 * Template function to print a queue of any type that contains another container
 */
template <typename T>
void printComplexQueue(queue<T> q) {
    while (!q.empty()) {
        // Access the front element (which is a container)
        const T& container = q.front();
        // Print the container's elements
        printContainer(container);
        cout << endl;
        q.pop();
    }
}

/*
 * Template function to print a map where the value is a container
 */
template <typename K, typename V>
void printComplexMap(const map<K, V>& m) {
    for (const auto& pair : m) {
        cout << pair.first << ": ";
        // Print the container stored as the value
        printContainer(pair.second);
        cout << endl;
    }
}


}

#endif /* UTIL_H_ */
