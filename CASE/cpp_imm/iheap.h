#ifndef IHEAP_H
#define IHEAP_H

#include "head.h"

// ... (iVector, iMap, KeyValue classes remain the same) ...
template <typename T>
class iVector {
public:
    unsigned int m_size;
    T* m_data;
    unsigned int m_num;

    iVector(unsigned int n = 100) {
        m_size = (n == 0) ? 100 : n;
        m_data = new T[m_size];
        m_num = 0;
    }
    ~iVector() {
        delete[] m_data;
    }
    void push_back(T d) {
        if (m_num == m_size) {
            re_allocate(m_size * 2);
        }
        m_data[m_num++] = d;
    }
    void re_allocate(unsigned int size) {
        if (size < m_num) return;
        T* tmp = new T[size];
        memcpy(tmp, m_data, sizeof(T) * m_num);
        m_size = size;
        delete[] m_data;
        m_data = tmp;
    }
    void clean() { m_num = 0; }
    T& operator[](unsigned int i) { return m_data[i]; }
};

template <typename T>
struct iMap {
    T* m_data;
    int m_num;
    std::vector<int> occur;
    const T SENTINEL = (T)-1; 

    iMap() : m_data(nullptr), m_num(0) {}
    ~iMap() { delete[] m_data; }

    void initialize(int n) {
        occur.clear();
        m_num = n;
        delete[] m_data;
        m_data = new T[m_num];
        std::fill(m_data, m_data + m_num, SENTINEL);
    }
    void clean() {
        for (int p : occur) {
            m_data[p] = SENTINEL;
        }
        occur.clear();
    }
    T get(int p) { return m_data[p]; }
    void erase(int p) { m_data[p] = SENTINEL; }
    bool notexist(int p) { return m_data[p] == SENTINEL; }
    void insert(int p, T d) {
        if (m_data[p] == SENTINEL) {
            occur.push_back(p);
        }
        m_data[p] = d;
    }
};

template <typename Key, typename Value>
struct KeyValue {
    Key key;
    Value value;
    KeyValue(const Key& k, const Value& v) : key(k), value(v) {}
    KeyValue() {}
};


template <typename Value>
struct iHeap {
    iMap<int> pos;
    iVector<KeyValue<int, Value>> m_data;

    void initialize(int n) {
        pos.initialize(n);
        m_data.clean();
    }

    void insert(int key, Value value) {
        if (pos.notexist(key)) {
            m_data.push_back(KeyValue<int, Value>(key, value));
            pos.insert(key, m_data.m_num - 1);
            up(m_data.m_num - 1);
        } else {
            int p = pos.get(key);
            m_data[p].value = value;
            if (p > 0 && value < m_data[(p - 1) / 2].value) {
                up(p);
            } else {
                down(p);
            }
        }
    }
    
    int top_key() {
        assert(m_data.m_num > 0);
        return m_data[0].key;
    }

    Value top_value() {
        assert(m_data.m_num > 0);
        return m_data[0].value;
    }

    int pop() {
        assert(m_data.m_num > 0);
        int top_key = m_data[0].key;
        pos.erase(top_key);
        if (m_data.m_num > 1) {
            m_data[0] = m_data[m_data.m_num - 1];
            pos.insert(m_data[0].key, 0);
            down(0);
        }
        m_data.m_num--;
        return top_key;
    }

    bool empty() { return (m_data.m_num == 0); }

private:
    void up(int p) {
        KeyValue<int, Value> x = m_data[p];
        for (; p > 0 && x.value < m_data[(p - 1) / 2].value; p = (p - 1) / 2) {
            m_data[p] = m_data[(p - 1) / 2];
            pos.insert(m_data[p].key, p);
        }
        m_data[p] = x;
        pos.insert(x.key, p);
    }
    void down(int p) {
        int child;
        KeyValue<int, Value> tmp = m_data[p];
        // 【修正】将 m_data.m_num 强制转换为有符号整数 (int)
        for (; (child = 2 * p + 1) < (int)m_data.m_num; p = child) {
            if (child + 1 < (int)m_data.m_num && m_data[child + 1].value < m_data[child].value) {
                child++;
            }
            if (m_data[child].value < tmp.value) {
                m_data[p] = m_data[child];
                pos.insert(m_data[p].key, p);
            } else {
                break;
            }
        }
        m_data[p] = tmp;
        pos.insert(m_data[p].key, p);
    }
};

#endif // IHEAP_H