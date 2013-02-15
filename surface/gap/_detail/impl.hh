#pragma once
#ifndef IMPL_QMMYE4OO
#define IMPL_QMMYE4OO

# include <boost/interprocess/managed_shared_memory.hpp>
# include <boost/interprocess/allocators/allocator.hpp>

# include <boost/interprocess/sync/interprocess_sharable_mutex.hpp>
# include <boost/interprocess/sync/sharable_lock.hpp>
# include <boost/interprocess/sync/scoped_lock.hpp>
# include <boost/interprocess/sync/scoped_lock.hpp>
# include <boost/interprocess/sync/interprocess_condition.hpp>

# include <boost/interprocess/containers/string.hpp>
# include <boost/interprocess/containers/vector.hpp>
# include <boost/interprocess/containers/list.hpp>
# include <boost/interprocess/containers/map.hpp>

# include <boost/algorithm/string.hpp>

# include <utility>

# include <surface/gap/State.hh>
# include <elle/Buffer.hh>

# include <elle/algorithm.hh>

# include <plasma/meta/Client.hh>
# include <plasma/trophonius/Client.hh>
# include <plasma/plasma.hh>

namespace surface {
namespace gap {
    
#define SHARED
    namespace ipc = boost::interprocess;

    struct SharedNetwork;
    struct SharedTransaction;

    using Networks              = ::plasma::meta::NetworksResponse;
    using SharedNetworkPtr      = std::unique_ptr<SharedNetwork>;
    using SharedTransactionPtr  = SharedTransaction*;

    // {{{ Containers
    using segment_manager = ipc::managed_shared_memory::segment_manager;
    template <class T>
    using shared_allocator = ipc::allocator<T, segment_manager>;
    using shared_string = ipc::basic_string<char, std::char_traits<char>, shared_allocator<char>>;
    template <class T>
    using shared_list = ipc::list<T, shared_allocator<T>>;
    template <class T>
    using shared_vector = ipc::vector<T, shared_allocator<T>>;
    template <class K, class V>
    using shared_map = ipc::map<K, V, std::less<K const>, ipc::allocator<std::pair<K const, V>, segment_manager>>; // bug using using
    // }}}

    // {{{ Locks
    using exclusive_lock = ipc::scoped_lock<ipc::interprocess_sharable_mutex>;
    using shared_lock = ipc::sharable_lock<ipc::interprocess_sharable_mutex>;
    // }}}

    std::string
    to_string(shared_string const &str);

    shared_string
    to_shared_string(std::string const &str, SharedStateManager &shm);

    template <typename Alloc>
    shared_string
    to_shared_string(std::string const &str, Alloc &alloc);

    // {{{ SharedStateManager
    struct SharedStateManager : public ipc::managed_shared_memory
    {
        SharedStateManager(ipc::create_only_t t,
                           const char *name,
                           size_type size)
            : ipc::managed_shared_memory(t, name, size)
        { }
        SharedStateManager(ipc::open_or_create_t t,
                           const char *name,
                           size_type size)
            : ipc::managed_shared_memory(t, name, size)
        { }
        SharedStateManager(ipc::open_only_t t,
                           const char *name)
            : ipc::managed_shared_memory(t, name)
        { }
        SharedStateManager(ipc::open_copy_on_write_t t,
                           const char *name)
            : ipc::managed_shared_memory(t, name)
        { }
    };
    // }}}

    // {{{ SharedNetwork
    struct SharedNetwork
    {
        SharedNetwork() = delete;
        SharedNetwork(SharedNetwork const &rhs) = default;
        SharedNetwork(Network const &n, SharedStateManager &shm)
            : _id{to_shared_string(n._id, shm), shm.get_segment_manager()}
            , _owner{to_shared_string(n.owner, shm), shm.get_segment_manager()}
            , _name{to_shared_string(n.name, shm), shm.get_segment_manager()}
            , _model{to_shared_string(n.model, shm), shm.get_segment_manager()}
            , _root_block{to_shared_string(n.root_block, shm), shm.get_segment_manager()}
            , _root_address{to_shared_string(n.root_address, shm), shm.get_segment_manager()}
            , _group_block{to_shared_string(n.group_block, shm), shm.get_segment_manager()}
            , _group_address{to_shared_string(n.group_address, shm), shm.get_segment_manager()}
            , _descriptor{to_shared_string(n.descriptor, shm), shm.get_segment_manager()}
            , _users{shm.get_segment_manager()}
            {
                for (auto const &user : n.users)
                {
                    this->_users.push_back(to_shared_string(user, shm));
                }
            }

        SharedNetwork(SharedStateManager &shm)
            : _id{shm.get_segment_manager()}
            , _owner{shm.get_segment_manager()}
            , _name{shm.get_segment_manager()}
            , _model{shm.get_segment_manager()}
            , _root_block{shm.get_segment_manager()}
            , _root_address{shm.get_segment_manager()}
            , _group_block{shm.get_segment_manager()}
            , _group_address{shm.get_segment_manager()}
            , _descriptor{shm.get_segment_manager()}
            , _users{shm.get_segment_manager()}
        {
        }

        explicit
        operator Network() const
        {
            Network n;
            n._id           = to_string(this->_id);
            n.owner         = to_string(this->_owner);
            n.name          = to_string(this->_name);
            n.model         = to_string(this->_model);
            n.root_block    = to_string(this->_root_block);
            n.root_address  = to_string(this->_root_address);
            n.group_block   = to_string(this->_group_block);
            n.group_address = to_string(this->_group_address);
            n.descriptor    = to_string(this->_descriptor);
            for (auto &user : this->_users)
            {
                n.users.push_back(to_string(user));
            }
            return n;
        }

        SharedNetwork &
        operator = (Network const &n)
        {
            auto alloc          = this->_id.get_allocator();
            this->_id           = to_shared_string(n._id, alloc);
            this->_owner        = to_shared_string(n.owner, alloc);
            this->_name         = to_shared_string(n.name, alloc);
            this->_model        = to_shared_string(n.model, alloc);
            this->_root_block   = to_shared_string(n.root_block, alloc);
            this->_root_address = to_shared_string(n.root_address, alloc);
            this->_group_block  = to_shared_string(n.group_block, alloc);
            this->_group_address= to_shared_string(n.group_address, alloc);
            this->_descriptor   = to_shared_string(n.descriptor, alloc);
            for (auto const &user : n.users)
            {
                this->_users.push_back(to_shared_string(user, alloc));
            }
            return *this;
        }

        ipc::interprocess_sharable_mutex mutable    _mutex;
     public:
        shared_string              _id;
        shared_string              _owner;
        shared_string              _name;
        shared_string              _model;
        shared_string              _root_block;
        shared_string              _root_address;
        shared_string              _group_block;
        shared_string              _group_address;
        shared_string              _descriptor;
        shared_list<shared_string> _users;
    };
    // }}}

    // {{{ SharedUser
    struct SharedUser
    {
        SharedUser() = default;
        SharedUser(SharedStateManager &shm)
            : _id{shm.get_segment_manager()}
            , _fullname{shm.get_segment_manager()}
            , _email{shm.get_segment_manager()}
            , _public_key{shm.get_segment_manager()}
            , _status{}
        {
            ELLE_DEBUG("Construct shared USER");
        }

        ~SharedUser()
        {
            ELLE_DEBUG("Destroy shared USER");
        }

        void
        construct_from(User const &u)
        {
            exclusive_lock l(this->_mutex);
            auto assign = [] (shared_string &dst, std::string const &src)
            {
                dst.assign(begin(src), end(src));
            };

            assign(this->_id, u._id);
            assign(this->_fullname, u.fullname);
            assign(this->_email, u.email);
            assign(this->_public_key, u.public_key);
            this->_status = u.status;
        }

        std::string
        id() const
        {
            shared_lock l(this->_mutex);
            return std::string{
                begin(this->_id),
                end(this->_id)
            };
        }

        void
        id(std::string const &str)
        {
            exclusive_lock l(this->_mutex);
            this->_id.assign(begin(str),
                             end(str));
        }

        std::string
        fullname() const
        {
            shared_lock l(this->_mutex);
            return std::string{
                begin(this->_fullname),
                end(this->_fullname)
            };
        }

        void
        fullname(std::string const &str)
        {
            exclusive_lock l(this->_mutex);
            this->_fullname.assign(begin(str),
                                   end(str));
        }
        
        std::string
        email() const
        {
            shared_lock l(this->_mutex);
            return std::string{
                begin(this->_email),
                end(this->_email)
            };
        }

        void
        email(std::string const &str)
        {
            exclusive_lock l(this->_mutex);
            this->_email.assign(begin(str),
                                end(str));
        }
        
        std::string
        public_key() const
        {
            shared_lock l(this->_mutex);
            return std::string{
                begin(this->_public_key),
                end(this->_public_key)
            };
        }

        void
        public_key(std::string const &str)
        {
            exclusive_lock l(this->_mutex);
            this->_public_key.assign(begin(str),
                                     end(str));
        }

        int
        status() const
        {
            shared_lock l(this->_mutex);
            return this->_status;
        }

        // Serialized accesses to the User
        ipc::interprocess_sharable_mutex mutable    _mutex;

     private:
        shared_string                               _id;
        shared_string                               _fullname;
        shared_string                               _email;
        shared_string                               _public_key;
        int                                         _status;
    };
    /// }}}

    // {{{ SharedTrophonius
    struct SharedTrophonius
    {
      SharedTrophonius(unsigned int const &refcnt, SharedStateManager &shm)
        : _mutex{}
        , _notifications{shm.get_segment_manager()}
        , _refcnt{refcnt}
        , _connected{false}
      {
      }

      // This function push the notification in shared memory
      // if N is devrived from plasma::trophonius::Notification
      template <typename N>
      typename std::enable_if<std::is_base_of<plasma::trophonius::Notification, N>::value>::type
      push(N& n, SharedStateManager &shm)
      {
        elle::Buffer b;
        auto alloc = shm.get_segment_manager();

        exclusive_lock(this->_mutex);
        b.writer() << n;
        char *mem = static_cast<char *>(alloc->allocate(b.size()));
        auto bytes = b.contents();
        for (unsigned int i = 0;
            i < b.size();
            ++i)
        {
          mem[i] = bytes[i];
        }
        this->_notifications.push_back(std::make_pair(n.notification_type, mem));
      }

    private:
      template <class T>
      plasma::trophonius::Notification*
      create_and_build(elle::Buffer &b, void *memory)
      {
        auto *n = new plasma::trophonius::UserStatusNotification();
        b.append(memory, sizeof(plasma::trophonius::UserStatusNotification));

        b.reader() >> *n;
        return n;
      }

    public:

      std::unique_ptr<plasma::trophonius::Notification>
      pop()
      {
        if (this->_notifications.size() == 0)
          return nullptr;
        std::pair<NotificationType, void *> head = this->_notifications.back();
        std::unique_ptr<plasma::trophonius::Notification> ptr;
        elle::Buffer b;

        using namespace plasma::trophonius;
        switch (head.first)
          {
            case NotificationType::user_status:
              ptr.reset(create_and_build<UserStatusNotification>(b, head.second));
              break;
            case NotificationType::transaction:
              ptr.reset(create_and_build<TransactionNotification>(b, head.second));
              break;
            case NotificationType::transaction_status:
              ptr.reset(create_and_build<TransactionStatusNotification>(b, head.second));
              break;
            case NotificationType::message:
              ptr.reset(create_and_build<MessageNotification>(b, head.second));
              break;
            case NotificationType::connection_enabled:
              ptr.reset(create_and_build<Notification>(b, head.second));
              break;
            case NotificationType::network_update:
              ptr.reset(create_and_build<NetworkUpdateNotification>(b, head.second));
              break;
            default:
              break;
          }
        return ptr;
      }

      ipc::interprocess_sharable_mutex mutable          _mutex;

      shared_vector<std::pair<NotificationType, void*>> _notifications;
      unsigned int const &                              _refcnt;
      bool                                              _connected;
    };
    // }}}

    // {{{ SharedTransaction
    struct SharedTransaction
    {
      SharedTransaction() = delete;
      SharedTransaction(SharedTransaction const &rhs) = default;
      SharedTransaction(Transaction const &n, SharedStateManager &shm)
        : _transaction_id{to_shared_string(n.transaction_id, shm),  shm.get_segment_manager()}
        , _sender_id{to_shared_string(n.sender_id, shm),  shm.get_segment_manager()}
        , _sender_fullname{to_shared_string(n.sender_fullname, shm),  shm.get_segment_manager()}
        , _sender_device_id{to_shared_string(n.sender_device_id, shm),  shm.get_segment_manager()}
        , _recipient_id{to_shared_string(n.recipient_id, shm),  shm.get_segment_manager()}
        , _recipient_fullname{to_shared_string(n.recipient_fullname, shm),  shm.get_segment_manager()}
        , _recipient_device_id{to_shared_string(n.recipient_device_id, shm),  shm.get_segment_manager()}
        , _recipient_device_name{to_shared_string(n.recipient_device_name, shm),  shm.get_segment_manager()}
        , _network_id{to_shared_string(n.network_id, shm),  shm.get_segment_manager()}
        , _message{to_shared_string(n.message, shm),  shm.get_segment_manager()}
        , _first_filename{to_shared_string(n.first_filename, shm),  shm.get_segment_manager()}
        , _files_count{n.files_count}
        , _total_size{n.total_size}
        , _is_directory{n.is_directory}
        , _status{n.status}
      {
      }

      explicit
      operator Transaction() const
      {
        Transaction n;

        n.transaction_id        = to_string(this->_transaction_id);
        n.sender_id             = to_string(this->_sender_id);
        n.sender_fullname       = to_string(this->_sender_fullname);
        n.sender_device_id      = to_string(this->_sender_device_id);
        n.recipient_id          = to_string(this->_recipient_id);
        n.recipient_fullname    = to_string(this->_recipient_fullname);
        n.recipient_device_id   = to_string(this->_recipient_device_id);
        n.recipient_device_name = to_string(this->_recipient_device_name);
        n.network_id            = to_string(this->_network_id);
        n.message               = to_string(this->_message);
        n.first_filename        = to_string(this->_first_filename);

        n.files_count           = this->_files_count;
        n.total_size            = this->_total_size;
        n.is_directory          = this->_is_directory;
        n.status                = this->_status;
        return n;
      }

      SharedTransaction &
      operator = (Transaction const &n)
        {
          auto allocator = this->_transaction_id.get_allocator();

          _transaction_id         = to_shared_string(n.transaction_id, allocator);
          _sender_id              = to_shared_string(n.sender_id, allocator);
          _sender_fullname        = to_shared_string(n.sender_fullname, allocator);
          _sender_device_id       = to_shared_string(n.sender_device_id, allocator);
          _recipient_id           = to_shared_string(n.recipient_id, allocator);
          _recipient_fullname     = to_shared_string(n.recipient_fullname, allocator);
          _recipient_device_id    = to_shared_string(n.recipient_device_id, allocator);
          _recipient_device_name  = to_shared_string(n.recipient_device_name, allocator);
          _network_id             = to_shared_string(n.network_id, allocator);
          _message                = to_shared_string(n.message, allocator);
          _first_filename         = to_shared_string(n.first_filename, allocator);

          _files_count            = n.files_count;
          _total_size             = n.total_size;
          _is_directory           = n.is_directory;
          _status                 = n.status;

          return *this;
        }

      ipc::interprocess_sharable_mutex mutable    _mutex;

    public:
      shared_string   _transaction_id;
      shared_string   _sender_id;
      shared_string   _sender_fullname;
      shared_string   _sender_device_id;
      shared_string   _recipient_id;
      shared_string   _recipient_fullname;
      shared_string   _recipient_device_id;
      shared_string   _recipient_device_name;
      shared_string   _network_id;
      shared_string   _message;
      shared_string   _first_filename;
      int             _files_count;
      int             _total_size;
      int             _is_directory;
      int             _status;
    };
    // }}}

    // {{{ SharedStates
    struct SharedStates
    {
        SharedStates(SharedStateManager &shm)
            : _mutex{}
            , _output_dir{shm.get_segment_manager()}
            , _refcnt{1}
            , _user_mutex{}
            , _me{shm}
            , _networks_mutex{}
            , _networks{std::less<shared_string const>{}, shm.get_segment_manager()}
            , _networks_dirty{true}
            , _device_mutex{}
            , _device_id{shm.get_segment_manager()}
            , _device_name{shm.get_segment_manager()}
            //, _tropho{this->_refcnt, shm}
                        
        {
        }

        void
        rejoin()
        {
            this->_refcnt++;
        }

        ~SharedStates()
        {
        }

        std::string
        output_dir() const
        {
            shared_lock l(this->_mutex);
            return std::string{
                begin(this->_output_dir),
                end(this->_output_dir)
            };
        }

        void
        output_dir(std::string const &str)
        {
            exclusive_lock l(this->_mutex);
            this->_output_dir.assign(begin(str), end(str));
        }

        unsigned int
        refcnt() const
        {
            shared_lock l(this->_mutex);
            return this->_refcnt;
        }

        std::string
        device_id()
        {
            shared_lock l(this->_device_mutex);
            return std::string{
                begin(this->_device_id),
                end(this->_device_id)
            };
        }

        std::string
        device_name()
        {
            shared_lock l(this->_device_mutex);
            return std::string{
                begin(this->_device_name),
                end(this->_device_name)
            };
        }

        void
        device_id(std::string const &str)
        {
            exclusive_lock l(this->_device_mutex);
            this->_device_id.assign(begin(str),
                                    end(str));
        }

        void
        device_name(std::string const &str)
        {
            exclusive_lock l(this->_device_mutex);
            this->_device_name.assign(begin(str),
                                      end(str));
        }

        User
        me() const
        {
            shared_lock l(this->_user_mutex);
            return User{
                this->_me.id(),
                this->_me.fullname(),
                this->_me.email(),
                this->_me.public_key(),
                this->_me.status(),
            };
        }

        bool
        networks_dirty()
        {
            shared_lock (this->_networks_mutex);
            return this->_networks_dirty;
        }

        void
        networks_dirty(bool val)
        {
            shared_lock l(this->_networks_mutex);
            this->_networks_dirty = val;
        }

        void
        lock_networks()
        {
            this->_networks_mutex.lock();
        }

        void
        unlock_networks()
        {
            this->_networks_mutex.unlock();
        }

        //Other info
        ipc::interprocess_sharable_mutex mutable        _mutex;
        shared_string                                   _output_dir;
        unsigned int                                    _refcnt;

        // User info
        ipc::interprocess_sharable_mutex mutable        _user_mutex;
        SharedUser                                      _me;

        // Network info
        ipc::interprocess_sharable_mutex mutable        _networks_mutex;
        shared_map<shared_string, SharedNetworkPtr>     _networks;
        bool                                            _networks_dirty;

        // Device info
        ipc::interprocess_sharable_mutex mutable        _device_mutex;
        shared_string                                   _device_id;
        shared_string                                   _device_name;

        // Notifications info
        // Trophonius sharing is troublesome, so I'll make it later
        // SharedTrophonius                             _tropho;

    };
    // }}}

    // {{{ utility
    inline
    shared_string
    to_shared_string(std::string const &str, SharedStateManager &shm)
    {
        shared_string tmp{shm.get_segment_manager()};

        tmp.assign(begin(str),
                   end(str));
        return move(tmp);
    }

    template <typename Alloc>
    shared_string
    to_shared_string(std::string const &str, Alloc &alloc)
    {
        shared_string tmp{alloc};

        tmp.assign(begin(str),
                   end(str));
        return move(tmp);
    }
    
    inline
    std::string
    to_string(shared_string const &str)
    {
        std::string tmp;

        tmp.assign(begin(str),
                   end(str));
        return move(tmp);
    }

    // }}}

#undef SHARED

} /* gap */
} /* surface */

#endif /* end of include guard: IMPL_QMMYE4OO */
