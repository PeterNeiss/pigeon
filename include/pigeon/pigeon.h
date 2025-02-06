/*
MIT License

Copyright (c) 2025 Peter Neiss 

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*
README:
pigeon is a single header C++11 library for single threaded message delivery.
It is similar to the observer pattern but cares about lifetime issues. 
Use cases are messages, events, signal&slot and publisher&subscriber.
Getting started by looking at the pigeon tutorial and the examples. 
Let the pigeons fly.
*/

#include <type_traits>
#include <utility>
#include <stdexcept>
#include <cstdint>

namespace pigeon 
{
  using size_t = decltype(sizeof(0));

  enum class who             {pigeon, message};
  enum class value_state     {original, changed, moved_from, constant};
  enum class iteration_state {dead, progress, finish, repeat}; 

  class  pigeon;
  struct global_access{};
  struct protected_access{};
  template <typename = void(), typename = global_access> class message;

  struct allocator
  {
    virtual      ~allocator ()              = default;
    virtual void* allocate  (size_t)        = 0;
    virtual void  deallocate(void*, size_t) = 0;
  };

  namespace detail { class contact; }
  class contact_token
  {
    public:
      explicit contact_token(detail::contact* ptr):contact(ptr) { }
    
    private:
      detail::contact* contact;
      friend class pigeon;
      template <typename, typename> friend class message;
  };

  namespace detail 
    // Identifiers in namespace detail are not meant for the user and are not part of the API
  {
    template <typename MR, typename RR> struct call_handler;
      // MR: Message Return type
      // RR: Response handler Return type
      // Use this metaprogramming class to call handlers with different signatures
      // detects if RR equals iteration_state, otherwise defaults to iteration_state::progress

    template<> struct call_handler<void, void>
    {
      template <typename H> static iteration_state call(H& h) { h(); return iteration_state::progress; }
    };
 
    template<> struct call_handler<void, iteration_state>
    {
      template <typename H> static iteration_state call(H& h) { return h(); }
    };

    template<typename T> struct call_handler<T, void>
    {
      template <typename H, typename R>
      static iteration_state call(H& h, R&& r) { h(std::forward<R>(r)); return iteration_state::progress; }
    };

    template<typename MR> struct call_handler<MR, iteration_state>
    {
      template <typename H, typename R>
      static iteration_state call(H& h, R&& r) { return h(std::forward<R>(r)); }
    };

    template <typename T>
    class flag_pointer
      // We use pointer_with_Flag to save memory, Cache, bandwidth and for better alignment
    {
        struct scoped_reset_guard{ flag_pointer* self; ~scoped_reset_guard() { self->reset(); } };

      public:
        flag_pointer(T* ptr = nullptr)       :PointerWithFlag{reinterpret_cast<std::uintptr_t>(ptr)} { }
        flag_pointer(flag_pointer const& ptr):PointerWithFlag{ptr.PointerWithFlag}                   { }

        flag_pointer& operator=    (flag_pointer const& rhs) = delete; 
        explicit      operator bool() const { return get() != nullptr; }
        T*            operator->   () const { return *this; }

        bool test() const { return PointerWithFlag &   static_cast<std::uintptr_t>(1); }
        void set()        {        PointerWithFlag |=  static_cast<std::uintptr_t>(1); } 
        void reset()      {        PointerWithFlag &= ~static_cast<std::uintptr_t>(1); }

        scoped_reset_guard scoped_set() { set(); return {this}; }
        
        T* get() const { return reinterpret_cast<T*>(PointerWithFlag & ~static_cast<std::uintptr_t>(1)); }

        void keep_flag_assign_pointer(flag_pointer const& rhs)
        {
          bool keep_flag = test();
          PointerWithFlag = rhs.PointerWithFlag;
          keep_flag ? set() : reset();
        }

      private:
        std::uintptr_t PointerWithFlag;
    };

    struct contact
    {
      virtual     ~contact()       = default;
      virtual void destruct()      = 0;
      virtual void callOnDrop(who) = 0;

      flag_pointer<contact> NextContact;  // flag stores dropped

      bool isDropped () const { return NextContact.test(); }
      void setDropped(who w)  { NextContact.set(); callOnDrop(w); }
      void drop      (who w)  { if (isDropped()) destruct(); else setDropped(w); }
    };

    template <typename R, typename ...Args>
    struct sender: contact
    {
      flag_pointer<sender> NextSender{nullptr};
        // We need NextSender to have the same type as Message::Senders
        // The flag is unused here

      virtual R send(Args ...args) = 0; 

      template <typename MR, typename H>
      typename std::enable_if<std::is_same<MR, void>::value, iteration_state>::type
        // Case where Message Handler has return type void
      do_send(H& h, Args ...args) 
      { 
        send(std::forward<Args>(args)...); 
        return call_handler<void, decltype(h())>::call(h); 
      }

      template <typename MR, typename H>
      typename std::enable_if<!std::is_same<MR, void>::value, iteration_state>::type
        // Case where Message Handler has not return type void
      do_send(H& h, Args ...args) 
      { 
        return detail::call_handler<MR, decltype(h(std::declval<MR>()))>::
          call(h, send(std::forward<Args>(args)...)); 
      }

      template <typename H>
      iteration_state try_send(H& h, Args ...args)
      {
        if (isDropped())
          return iteration_state::dead;
        else
          return do_send<R>(h, std::forward<Args>(args)...);
      }
    };

    template <typename H, typename F, typename R, typename ...Args>
    struct inbox: H, F, sender<R, Args...>
      // Derive from H to enable empty base class optimization if possible
    {
      template <typename I, typename J>
      inbox(I&& box, J&& drop):H{std::forward<I>(box)}, F{std::forward<J>(drop)} { }
      R send(Args ...args) override { return H::operator()(std::forward<Args>(args)...); }

      void destruct() override { delete this; }
      void callOnDrop(who w) override { F::operator()(contact_token{this}, w); }
    };

    template <typename H, typename F, typename R, typename ...Args>
    struct inbox_with_allocator: public inbox<H, F, R, Args...>
    {
      allocator* Alloc;

      template <typename I, typename J>
      inbox_with_allocator(I&& box, allocator* alloc, J&& drop)
       :inbox<H, F, R, Args...>{std::forward<I>(box), std::forward<J>(drop)},
        Alloc{alloc}
      { }

      void destruct() override
      { 
        auto alloc = Alloc;
        this->~inbox_with_allocator();
        alloc->deallocate(this, sizeof (inbox_with_allocator)); 
      }
    };

    struct noop { void operator()(contact_token, who) { } };

    template <typename R, typename F>
    struct handler
    {
      R* self;
      F f;

      template <typename ...Args>
      auto operator()(Args&& ...args)
        -> decltype((self->*f)(std::forward<Args>(args)...))
      { return (self->*f)(std::forward<Args>(args)...); }
    };

    template <typename ...Args> struct argument_checker;

    template <> struct argument_checker<> 
    { static const bool value = true; };

    template <typename Arg, typename ...Args>
    struct argument_checker<Arg, value_state&, Args...>: argument_checker<Args...> { };

    template <typename Arg, typename ...Args>
    struct argument_checker<Arg, Args...>: argument_checker<Args...>
    { 
      static_assert(!std::is_rvalue_reference<Arg>::value, 
        "RValue reference not allowed as arguments without a "
        "following associated pigeon::value_state& argument"
      ); 

      static_assert(!std::is_lvalue_reference<Arg>::value ||
        std::is_const<typename std::remove_reference<Arg>::type>::value, 
        "LValue reference must be const or followed by "
        "an associated pigeon::value_state& argument"
      ); 
    };

  } // namespace detail

  template <typename R, typename ...Args>
  class message<R(Args...), protected_access>
  { 
    static_assert(detail::argument_checker<Args...>::value, 
      "Check Arguments for non-const references."
      "A corresponding pigeon::value_type must follow each such argument!"
    );

    public:
     ~message() { clear(); }
      bool isSending() const { return Senders.test(); }

    protected: 
      size_t size() const
      {
        size_t counter{0};
        auto sender = Senders.get();
        while(sender)
        {
          if (not sender->isDropped())
            ++counter;

          sender = sender->NextSender.get();
        }
        return counter;
      }

      void ensureNotSending() const
      {
        if (isSending())
          throw std::logic_error("Logic error while delivering");
      }

      void clear()
      {
        ensureNotSending(); 
        auto sender = Senders.get();
        Senders.keep_flag_assign_pointer(nullptr);
        while(sender)
        {
          auto next = sender->NextSender.get();
          sender->drop(who::message);
          sender = next;
        }
      }

      bool drop(contact_token token)
      {
        ensureNotSending(); 
        auto previous_sender = &Senders;
        auto sender = Senders.get();
        while(sender)
        {
          if (sender == token.contact)
          {
            auto next = sender->NextSender.get();
            previous_sender->keep_flag_assign_pointer(next);
            sender->drop(who::message);
            return true;
          }
          else
          {
            previous_sender = &sender->NextSender;
            sender          =  sender->NextSender.get();
          }
        }
        return false;
      }

      template <typename H>
      void response(Args...args, H&& h) 
      { 
        // We purposely silently ignore reentrant responding through user provided handlers
        if (isSending())
          return;

        // set isSending true here in exception safe RAII fashion
        auto guard = Senders.scoped_set();

        auto previous_sender = &Senders;
        auto sender = Senders.get();
        while(sender)
        {
          switch(sender->try_send(h, std::forward<Args>(args)...))
          {
            case iteration_state::dead:
            {
              auto next = sender->NextSender.get();
              previous_sender->keep_flag_assign_pointer(next); 
              sender->drop(who::message);
              sender = next;
              break;
            }

            case iteration_state::progress:
              previous_sender = &sender->NextSender;
              sender = sender->NextSender.get();
              break;

            case iteration_state::repeat:
            {
              // Do not change the order of these steps without intense scrutiny
              // Modify linked list so we will repeat the OTHER senders, but
              // not the active sender
              
              // find lastSender
              auto lastSender = sender;
              while(lastSender->NextSender)
                lastSender = lastSender->NextSender.get();

              // unlink sender and make new list end 
              previous_sender->keep_flag_assign_pointer(nullptr);

              // splice
              lastSender->NextSender.keep_flag_assign_pointer(Senders);
              Senders.keep_flag_assign_pointer(sender);

              // next
              previous_sender = &sender->NextSender;
              sender = sender->NextSender.get();
              break;
            }

            case iteration_state::finish:
              return;
          }
        }
      }

      void send(Args ...args) 
      { response(std::forward<Args>(args)... , [](...){ }); }

    private:
      friend class pigeon;

      using sender_type = detail::sender<R, Args...>;

      detail::flag_pointer<sender_type> Senders;

      template<typename H, typename F>
      detail::contact* make_contact(H&& handler, allocator* alloc, F&& f)
      {
        ensureNotSending(); 

        auto sender = [alloc, &handler, &f] () -> sender_type*
        {
          using handler_type = typename std::remove_reference<H>::type;
          using drop_type    = typename std::remove_reference<F>::type;

          if (alloc)
          {
            using inbox_type = typename detail::inbox_with_allocator<handler_type, drop_type, R, Args...>;
            auto space = alloc->allocate(sizeof (inbox_type));
            return new (space) inbox_type{std::forward<H>(handler), alloc, std::forward<F>(f)};
          }
          else
           return new detail::inbox<handler_type, drop_type, R, Args...>{std::forward<H>(handler), std::forward<F>(f)};
        }();

        // Messages get delivered in reverse order of deliver calls 
        // which might be counter intuitive, but I do not guarantee
        // any order and even change it with iteration_state::repeat
        sender->NextSender.keep_flag_assign_pointer(Senders);
        Senders.keep_flag_assign_pointer(sender);
        return sender;
      }
  };

  template <typename R, typename ...Args>
  struct message<R(Args...), global_access>: message<R(Args...), protected_access>
  { 
    using base = message<R(Args...), protected_access>;
    using base::size;
    using base::clear;
    using base::drop;
    using base::response;
    using base::send;
  };

  template <typename R, typename ...Args, typename F>
  class message<R(Args...), F>: public message<R(Args...), protected_access>
  { 
    protected:
      friend F;

      using base = message<R(Args...), protected_access>;
      using base::size;
      using base::clear;
      using base::drop;
      using base::response;
      using base::send;
  };

  namespace detail { template <typename> class deliver_proxy; }
  class pigeon
  {
    public:
      pigeon() = default;
     ~pigeon() { setDestructing(); clear(); }

      size_t size() const
      {
        ensureNotDestructing();
        size_t counter{0};
        auto contact = contacts.get();
        while(contact)
        {
          if (not contact->isDropped())
            ++counter;

          contact = contact->NextContact.get();
        }
        return counter;
      }

      void clear()
      {
        auto contact = contacts.get();
        contacts.keep_flag_assign_pointer(nullptr);
        while(contact)
        {
          auto next = contact->NextContact.get();
          contact->drop(who::pigeon);
          contact = next;
        }
      }

      template <typename M, typename I, typename F = detail::noop>
      contact_token deliver(M& message, I&& inbox, allocator* alloc = nullptr, F&& f = F{}) 
      {
        ensureNotDestructing();
        auto contact = message.make_contact(std::forward<I>(inbox), alloc, std::forward<F>(f));
        contact->NextContact.keep_flag_assign_pointer(contacts);
        contacts.keep_flag_assign_pointer(contact);
        return contact_token{contact};
      } 

      template <typename M>
      detail::deliver_proxy<M> deliver(M& message) 
      {
        return {*this, message};
      }

      bool drop(contact_token token)
      {
        auto previous_contact = &contacts;
        auto contact = contacts.get();
        while(contact)
        {
          if (contact == token.contact)
          {
            auto next = contact->NextContact;
            previous_contact->keep_flag_assign_pointer(next);
            contact->drop(who::pigeon);
            return true;
          }
          else
          {
            // continue search
            previous_contact = &contact->NextContact;
            contact = contact->NextContact.get();
          }
        }
        return false;
      }

    private:
      // Because of previous_contact, the type of contacts must match NextContact
      detail::flag_pointer<detail::contact> contacts;
      void setDestructing() { contacts.set(); }
      void ensureNotDestructing() const
      {
        if (contacts.test())
          throw std::logic_error("Logic error while destructing pigeon::pigeon");
      }
  };

  template <typename R, typename P = pigeon>
  class receiver
  {
    public:
      template <typename M, typename RR, typename ...Args>
      void deliver(M& msg, RR(R::*f)(Args...))
      { pigeon.deliver(msg, detail::handler<R, RR(R::*)(Args...)>{static_cast<R*>(this), f}); }

      template <typename M, typename H>
      void deliver(M& msg, H&& h)
      { pigeon.deliver(msg, std::forward<H>(h)); }

    protected:
      P pigeon;
  };

  namespace detail
  {
    template <typename, typename> class deliver_onDrop_helper;

    template <typename M>
    class deliver_proxy
    {
      public:
        deliver_proxy(pigeon& pigeon, M& message, allocator* alloc = nullptr)
         : Pigeon(pigeon), Message(message), Allocator(alloc) { }

        deliver_proxy(deliver_proxy const&) = default;

        deliver_proxy& withAllocator(allocator* alloc)
        { Allocator = alloc; return *this; }

        template <typename I, typename F = detail::noop>
        contact_token to(I&& box, F&& onDrop = F{})
        { return Pigeon.deliver(Message, std::forward<I>(box), Allocator, std::forward<F>(onDrop)); }

        template <typename F>
        deliver_onDrop_helper<M, F> onDrop(F&& f) 
        { return {*this, std::forward<F>(f)}; }

      private:
        pigeon& Pigeon;
        M& Message;
        allocator* Allocator;
    };

    template <typename M, typename F> 
    class deliver_onDrop_helper: public deliver_proxy<M>
    {
        using Base = deliver_proxy<M>;

      public:
        deliver_onDrop_helper(Base const& helper, F&& ff)
         :Base(helper), f(std::forward<F>(ff)) { }

        deliver_onDrop_helper(pigeon& pigeon, M& message, allocator* alloc, F&& ff)
         :Base(pigeon, message, alloc), f(std::forward<F>(ff)) { }

        template <typename I>
        contact_token to(I&& box)
        { return Base::to(std::forward<I>(box), std::forward<F>(f)); }
       
      private:
        using deliver_proxy<M>::onDrop;
        F f; 
    };

    template <typename M>
    struct onDrop_handler 
    { 
      M& Message;
      void operator()(contact_token token, who w) 
      { 
        if (w == who::pigeon)
          Message.drop(token); 
      }
    };

  } // namespace detail

  template<size_t N>
  struct arena_heap_allocator: allocator
  {
    static const size_t MinAlignment = sizeof(void*);
   
    arena_heap_allocator():Memory(new unsigned char[N]) { }
   ~arena_heap_allocator() { delete[] Memory; }

    size_t capacity() const { return N; }

    void* allocate(size_t size_bytes) override
    {
      auto oldByteIndex = ByteIndex;
      ByteIndex += ((size_bytes + MinAlignment - 1)/ MinAlignment) * MinAlignment;
      if (ByteIndex >= N)
        throw std::logic_error("Not enough memory");

      return &Memory[oldByteIndex]; 
    }

    void deallocate(void*, size_t) override { }

    unsigned char* Memory;
    size_t ByteIndex{0};
  };

  template<size_t N>
  struct arena_stack_allocator: allocator
  {
    static const size_t MinAlignment = sizeof(void*);

    size_t capacity() const { return N; }
   
    void* allocate(size_t size_bytes) override
    {
      auto oldByteIndex = ByteIndex;
      ByteIndex += ((size_bytes + MinAlignment - 1)/ MinAlignment) * MinAlignment;
      if (ByteIndex >= N)
        throw std::logic_error("Not enough memory");

      return &Memory[oldByteIndex]; 
    }

    void deallocate(void*, size_t) override { }

    unsigned char Memory[N];
    size_t ByteIndex{0};
  };

  template<typename A>
  class allocator_pigeon: pigeon
  {
      using base = pigeon;

    public:
      ~allocator_pigeon() { clear(); }

      using base::size;
      using base::clear;
      using base::drop;

      template <typename M>
      detail::deliver_onDrop_helper<M, detail::onDrop_handler<M>> deliver(M& message) 
      {
        return {*this, message, &allocator, detail::onDrop_handler<M>{message}};
      }

      template <typename M, typename I>
      contact_token deliver(M& message, I&& box) 
      {
        // When the allocator_pigeon goes away, the memory with the contact information goes away too!
        // We have to delete them properly, what means the message has to drop the contact to.
        // The onDrop_handler does this with the onDrop callback mechanism
        return base::deliver(message, std::forward<I>(box), &allocator, detail::onDrop_handler<M>{message});
      }

      size_t total_memory    () const { return allocator.capacity(); }
      size_t used_memory     () const { return allocator.ByteIndex; }
      size_t available_memory() const { return total_memory() - used_memory(); }

    private:
      A allocator;
  };
} // namespace pigeon

