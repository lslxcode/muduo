#include <iostream>
#include <boost/function.hpp>
#include <boost/bind.hpp>
using namespace std;
class Foo
{
public:
	void memberFunc(double d, int i, int j)
	{
		cout << d << endl;//打印0.5
		cout << i << endl;//打印100       
		cout << j << endl;//打印10
	}
};
int main()
{
	Foo foo;
	boost::function<void (int, int)> fp = boost::bind(&Foo::memberFunc, &foo, 0.5, _1, _2);
	fp(100, 200);//等价于 (&Foo)->memberFunc(0.5,100,200);
	boost::function<void (int, int)> fp2 = boost::bind(&Foo::memberFunc, boost::ref(foo), 0.5, _1, _2);
	fp2(55, 66); // ref() 强制按引用，bind()默认是按值传递，若要强制按引用，则使用ref()
	return 0;
}

