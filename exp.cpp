#include <iostream>
#include <cmath>
using namespace std;

int main()
{
    double t = 0.7;
    unsigned int i = 0;
    double a, b;
    do
    {
        i++;
        a = exp(-t);
        b = exp(-t) + exp(-t*i);
    }
    while(a != b);
    cout << i << endl;
    return 0;
}
