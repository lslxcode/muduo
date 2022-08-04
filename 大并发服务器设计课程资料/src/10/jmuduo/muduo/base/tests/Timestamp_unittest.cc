#include <muduo/base/Timestamp.h>
#include <vector>
#include <stdio.h>

using muduo::Timestamp;

void passByConstReference(const Timestamp& x)
{
  printf("%s\n", x.toString().c_str());
}

void passByValue(Timestamp x)
{
  printf("%s\n", x.toString().c_str());
}

void benchmark()
{
  const int kNumber = 1000*1000;

  std::vector<Timestamp> stamps;
  stamps.reserve(kNumber); /// 预留空间
  for (int i = 0; i < kNumber; ++i)
  {/// 插入10000000个时间
    stamps.push_back(Timestamp::now());
  }
  printf("%s\n", stamps.front().toString().c_str());
  printf("%s\n", stamps.back().toString().c_str());
  /// 计算时间差
  printf("%f\n", timeDifference(stamps.back(), stamps.front()));

  int increments[100] = { 0 };
  int64_t start = stamps.front().microSecondsSinceEpoch();
  for (int i = 1; i < kNumber; ++i)
  {
    int64_t next = stamps[i].microSecondsSinceEpoch();
    int64_t inc = next - start;
    start = next;
    if (inc < 0) /// 时间倒转了，系统时钟有问题
    {
      printf("reverse!\n");
    }
    else if (inc < 100)
    {
      ++increments[inc];
    }
    else
    {
      printf("big gap %d\n", static_cast<int>(inc));
    }
  }
	/// 小于100 个时间差有多少个
  for (int i = 0; i < 100; ++i)
  {
    printf("%2d: %d\n", i, increments[i]);
  }
}

int main()
{
	/// 构造一个时间戳对象，初始化时间戳为当前的时间，now是Timestamp类下的一个静态的成员，返回当前的时间
  Timestamp now(Timestamp::now());
  /// 返回当前的时间
  printf("%s\n", now.toString().c_str());
  /// 测试值传递
  passByValue(now);
  /// 测试引用传递
  passByConstReference(now);
  benchmark();
}

