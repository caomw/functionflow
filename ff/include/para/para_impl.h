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
#ifndef FF_PARA_PARA_IMPL_H_
#define FF_PARA_PARA_IMPL_H_

#include "common/common.h"
#include "runtime/taskbase.h"
#include "runtime/rtcmn.h"
#include "common/log.h"

namespace ff {
namespace internal
{
template <class RT>
class para_impl;


template <class T>
class para_ret {
public:
    para_ret(para_impl<T> & p)
        : m_refP(p)
        , m_oValue() {}

    T & get() {
        return m_oValue;
    }
    void set(T& v) {
        m_oValue = v;
    }
    void set(T&& v) {
        m_oValue = v;
    }

protected:
    para_impl<T> &      m_refP;
    T m_oValue;
};//end class para_ret;


template<class RT>
class para_impl : public ff::rt::task_base
{
public:
    template <class F>
    para_impl(F && f)
        : ff::rt::task_base(TKind::user_t)
        , m_oRet(*this)
        , m_oFunc(std::move(f))
        , m_iES(exe_state::exe_init){}

    virtual ~para_impl()
    {}
    virtual void	run()
    {
        m_iES = exe_state::exe_run;
        m_oRet.set(m_oFunc());
        m_iES.store(exe_state::exe_over);
    }
    RT & get() {
        return m_oRet.get();
    }


    exe_state	get_state()
    {
        return m_iES.load();
    }
    bool	check_if_over()
    {
        if(m_iES.load() == exe_state::exe_over)
            return true;
        return false;
    }
protected:
    para_ret<RT>	m_oRet;
    std::function<RT ()> m_oFunc;
    volatile std::atomic<exe_state>  m_iES;
};//end class para_impl

template<>
class para_impl<void> : public ff::rt::task_base
{
public:
    template< class F>
    para_impl(F && f)
        : ff::rt::task_base(TKind::user_t)
        , m_iES(exe_state::exe_init)
        , m_oFunc(std::move(f))
    {}

    virtual ~para_impl()
    {
    }

    virtual void	run()
    {
        //LOG_INFO(para)<<"para_impl::run(), "<<this;
        m_iES = exe_state::exe_run;
        m_oFunc();
        m_iES.store(exe_state::exe_over);
    }


    exe_state	get_state()
    {
        return m_iES.load();
    }
    bool	check_if_over()
    {
        if(m_iES.load() == exe_state::exe_over)
            return true;
        return false;
    }
protected:
    volatile std::atomic<exe_state>  m_iES;
    std::function<void ()> m_oFunc;
};//end class para_impl_ptr

#if 0
template<class RT>
using para_impl_ptr = para_impl<RT> *;

template<class ret_type, class F>
auto make_para_impl(F && f)
-> typename std::enable_if<
std::is_void<ret_type>::value,
    internal::para_impl_ptr<ret_type>
    >::type
{
    return new para_impl<ret_type>(std::forward<F>(f));
}
template<class ret_type, class F>
auto make_para_impl(F&& f)
-> typename std::enable_if<
!std::is_void<ret_type>::value,
internal::para_impl_ptr<ret_type>
>::type
{
    return new para_impl<ret_type>(std::forward<F>(f));
}
#endif

#if 1
template<class RT>
using para_impl_ptr = std::shared_ptr<para_impl<RT>>;


template <class ret_type, class F>
auto make_para_impl(F&& f)
-> typename std::enable_if<
std::is_void<ret_type>::value,
    internal::para_impl_ptr<ret_type>
    >::type
{
    auto p = std::make_shared<internal::para_impl<ret_type> >(std::forward<F>(f));
    _DEBUG(LOG_INFO(para)<<"generate a para task: 1 "<<p.get())
    return p;
}
template <class ret_type, class F>
auto make_para_impl(F&& f)
-> typename std::enable_if<
!std::is_void<ret_type>::value,
internal::para_impl_ptr<ret_type>
>::type
{
    auto p = std::make_shared<internal::para_impl<ret_type> >(std::forward<F>(f));
    _DEBUG(LOG_INFO(para)<<"generate a para task: 2 "<<p.get())
    return p;
}
#endif

template<class WT>
class para_impl_wait : public ff::rt::task_base
{
public:
    template<class RT>
    para_impl_wait(WT &  w, const para_impl_ptr<RT> & p)
        : ff::rt::task_base(TKind::user_t)
        , m_iES(exe_state::exe_init)
        , m_pFunc(std::dynamic_pointer_cast<ff::rt::task_base>(p))
        //, m_pFunc(p)
        , m_oWaitingPT(w) {}

    template<class RT>
    para_impl_wait(WT && w, const para_impl_ptr<RT> & p)
        : ff::rt::task_base(TKind::user_t)
        , m_pFunc(p)
        , m_oWaitingPT(w) {}

    virtual ~para_impl_wait()
    {
    }
    virtual void run()
    {
        m_iES=exe_state::exe_run;
        if(m_oWaitingPT.get_state() != exe_state::exe_over)
        {
            ::ff::rt::yield_and_ret_until([this]() {
                return m_oWaitingPT.check_if_over();
            });
        }
        m_pFunc->run();
        m_iES.store(exe_state::exe_over);
    }

    exe_state	get_state()
    {
        return m_iES.load();
    }
    bool	check_if_over()
    {
        if(m_iES.load() == exe_state::exe_over)
            return true;
        return false;
    }
protected:
    volatile std::atomic<exe_state> m_iES;
    ff::rt::task_base_ptr 	m_pFunc;
    WT  	m_oWaitingPT;
};//end class para_impl_wait;
template<class WT>
//using para_impl_wait_ptr = para_impl_wait<WT> *;
using para_impl_wait_ptr = std::shared_ptr<para_impl_wait<WT> >;

template<class RT>
void	schedule(para_impl_ptr<RT>  p)
{
    _DEBUG(LOG_INFO(rt)<<"schedule start ")
    ::ff::rt::schedule(std::dynamic_pointer_cast<ff::rt::task_base>(p));
//    ::ff::rt::schedule(p);
    _DEBUG(LOG_INFO(rt)<<"schedule end ")
}
template<class WT>
void	schedule(para_impl_wait_ptr<WT>  p)
{
    ::ff::rt::schedule(std::dynamic_pointer_cast<ff::rt::task_base>(p));
//  ::ff::rt::schedule(p);
}
#ifdef USING_MIMO_QUEUE
template<class RT>
void	schedule(para_impl_ptr<RT>  p, int32_t thrd_id)
{
    ::ff::rt::schedule(std::dynamic_pointer_cast<ff::rt::task_base>(p), thrd_id);
//    ::ff::rt::schedule(p);
}
template<class WT>
void	schedule(para_impl_wait_ptr<WT>  p, int32_t thrd_id)
{
    ::ff::rt::schedule(std::dynamic_pointer_cast<ff::rt::task_base>(p), thrd_id);
//  ::ff::rt::schedule(p);
}
#endif
}//end namespace internal
}//end namespace ff
#endif

