#include <iostream>

#include "dynamic_array.hpp"

int main()
{
    jd::Array<int> a;

    for (int i = 1; i <= 10; ++i)
        a.insert(i);

    for (int i = 0; i < a.size(); ++i)
        a[i] *= 2;

    for (auto it = a.begin(); it.hasNext(); it.next())
        std::cout << it.get() << std::endl;
}
