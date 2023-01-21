#include "BoundedBuffer.h"

using namespace std;


BoundedBuffer::BoundedBuffer (int _cap) : cap(_cap) {
    // modify as needed
}

BoundedBuffer::~BoundedBuffer () {
    // modify as needed
}

void BoundedBuffer::push (char* msg, int size) {
    // Create a unique lock then call wait on the condition variable, which will acquire the lock
    unique_lock UL = unique_lock(mu);

    // 1. Convert the incoming byte sequence given by msg and size into a vector<char>
    vector<char> charVec;
    for (int i = 0; i < size; i++){
        charVec.push_back(msg[i]);
    }
    //charVec.push_back('\0');

    // 2. Wait until there is room in the queue (i.e., queue length is less than cap)
    cvProd.wait(UL, [this]{return this->size() < (size_t)cap;});
    
    // 3. Then push the vector at the end of the queue
    q.push(charVec);

    // unlock the unique lock
    UL.unlock();
    // 4. Wake up threads that were waiting for push/we might not be empty anymore
    cvConsumer.notify_all();
}

int BoundedBuffer::pop (char* msg, int size) {
    unique_lock UL = unique_lock(mu);

    // 1. Wait until the queue has at least 1 item
    cvConsumer.wait(UL, [this]{return this->size() > 0;});

    // 2. Pop the front item of the queue. The popped item is a vector<char>
    vector<char> itemPopped = q.front();
    q.pop();

    // 3. Convert the popped vector<char> into a char*, copy that into msg; assert that the vector<char>'s length is <= size

    if (itemPopped.size() <= (size_t)size){
        for (size_t i = 0; i < itemPopped.size(); i++){
            msg[i] = itemPopped.at(i);
        }
    }

    // Unlock the unique lock
    UL.unlock();
    // 4. Wake up threads that were waiting for pop
    cvProd.notify_all();
    // 5. Return the vector's length to the caller so that they know how many bytes were popped
    return itemPopped.size();
}

size_t BoundedBuffer::size () {
    return q.size();
}
