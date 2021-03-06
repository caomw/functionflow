/***********************************************
The MIT License (MIT)

Copyright (c) 2012 Athrun Arthur <athrunarthur@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*************************************************/
#include "runtime/ring_buff.h"
#include "runtime/mutex_steal_queue.h"

#include "runtime/threadpool.h"

#include <vector>
#include <ctime>
#include <mutex>
#include <chrono>
#include <algorithm>
using namespace ff::rt;
typedef nonblocking_stealing_queue<int, 8> work_stealing_queue;
//typedef mutex_stealing_queue<int> work_stealing_queue;


#define CONCURRENCY 4
work_stealing_queue   g_q;
std::vector<int> g_nums;

std::atomic_bool quit(false);

std::mutex lock;

int64_t total = 0;
void runner()
{
  set_local_thrd_id(0);
  while(!quit)
  {
    int v = std::rand()%100;
    if(v == 0)
      continue;
    total += v;
    for(int i = 0; i < v; ++i)
    {
      int b = std::rand();
      lock.lock();
      g_nums.push_back(b);
      lock.unlock();
      g_q.push_back(b);
    }
    
    int e = std::rand()%v;
    std::vector<int> buf;
    for(int i = 0; i < e; ++i)
    {
      int b ;
      if(g_q.pop(b))
      {
	buf.push_back(b);
      }
    }
    
    lock.lock();
    for(int i = 0; i < buf.size(); i++)
    {
      bool done = false;
      for(auto it = g_nums.begin(); it != g_nums.end(); it++)
      {
	if(*it == buf[i])
	{
	  g_nums.erase(it);
	  done =true;
	  break;
	}
      }
      if(!done)
      {
	std::cout<<"Error! in runner--erase(), as the num:"<<buf[i]<<" is not in nums! " <<"buf size:"<<buf.size()<<", nums:"<<g_nums.size()<<std::endl;
      }
    }
    lock.unlock();
    
    std::chrono::milliseconds dura( 1 );
    std::this_thread::sleep_for( dura );
  }
  std::cout<<"r quit!"<<std::endl;
}

void steal(int i)
{
  set_local_thrd_id(i);
  while(!quit)
  {
    std::chrono::milliseconds dura(2);
    std::this_thread::sleep_for(dura);
    int v = std::rand()%100;
    
    int e = std::rand()%100;
    std::vector<int> buf;
    int b;
    while(g_q.steal(b))
    {
      buf.push_back(b);
    }

    lock.lock();
    for(int i = 0; i < buf.size(); i++)
    {
      bool done = false;
      for(auto it = g_nums.begin(); it != g_nums.end(); it++)
      {
	if(*it == buf[i])
	{
	  g_nums.erase(it);
	  done =true;
	  break;
	}
      }
      if(!done)
      {
	std::cout<<"Error! in steal--erase(), as the num:"<<buf[i]<<" is not in nums"<<"buf size:"<<buf.size()<<", nums:"<<g_nums.size()<<std::endl;
      }
    }
    lock.unlock();
  }
  std::cout<<"s:"<<i<<" quit"<<std::endl;
}
int main(int argc, char *argv[])
{
  std::srand(std::time(0)); // use current time as seed for random generator
  threadpool tp;
  tp.run(runner);
  tp.run([](){steal(1);});
  tp.run([](){steal(2);});
  tp.run([](){steal(3);});
  tp.run([](){steal(4);});
  
  std::chrono::seconds dura( 15 );
  std::this_thread::sleep_for( dura );
   quit.store(true);
   std::cout<<"wait all threads quiting..."<<std::endl;
   tp.join();
   std::cout<<"total nums:"<<total<<std::endl;
   std::cout<<"all quit!"<<std::endl;
   
  return 0;
}

