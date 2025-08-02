#ifndef TOML11_ORDERED_MAP_HPP
#define TOML11_ORDERED_MAP_HPP

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace toml
{

namespace detail
{
template<typename Cmp>
struct ordered_map_ebo_container
{
    Cmp cmp_; // empty base optimization for empty Cmp type
};
} // detail

template<typename Key, typename Val, typename Cmp = std::equal_to<Key>,
         typename Allocator = std::allocator<std::pair<Key, Val>>>
class ordered_map : detail::ordered_map_ebo_container<Cmp>
{
  public:
    using key_type    = Key;
    using mapped_type = Val;
    using value_type  = std::pair<Key, Val>;

    using key_compare    = Cmp;
    using allocator_type = Allocator;

    using container_type  = std::vector<value_type, Allocator>;
    using reference       = typename container_type::reference;
    using pointer         = typename container_type::pointer;
    using const_reference = typename container_type::const_reference;
    using const_pointer   = typename container_type::const_pointer;
    using iterator        = typename container_type::iterator;
    using const_iterator  = typename container_type::const_iterator;
    using size_type       = typename container_type::size_type;
    using difference_type = typename container_type::difference_type;

  private:

    using ebo_base = detail::ordered_map_ebo_container<Cmp>;

  public:

    ordered_map() = default;
    ~ordered_map() = default;
    ordered_map(const ordered_map&) = default;
    ordered_map(ordered_map&&)      = default;
    ordered_map& operator=(const ordered_map&) = default;
    ordered_map& operator=(ordered_map&&)      = default;

    ordered_map(const ordered_map& other, const Allocator& alloc)
        : container_(other.container_, alloc)
    {}
    ordered_map(ordered_map&& other, const Allocator& alloc)
        : container_(std::move(other.container_), alloc)
    {}

    explicit ordered_map(const Cmp& cmp, const Allocator& alloc = Allocator())
        : ebo_base{cmp}, container_(alloc)
    {}
    explicit ordered_map(const Allocator& alloc)
        : container_(alloc)
    {}

    template<typename InputIterator>
    ordered_map(InputIterator first, InputIterator last, const Cmp& cmp = Cmp(), const Allocator& alloc = Allocator())
        : ebo_base{cmp}, container_(first, last, alloc)
    {}
    template<typename InputIterator>
    ordered_map(InputIterator first, InputIterator last, const Allocator& alloc)
        : container_(first, last, alloc)
    {}

    ordered_map(std::initializer_list<value_type> v, const Cmp& cmp = Cmp(), const Allocator& alloc = Allocator())
        : ebo_base{cmp}, container_(std::move(v), alloc)
    {}
    ordered_map(std::initializer_list<value_type> v, const Allocator& alloc)
        : container_(std::move(v), alloc)
    {}
    ordered_map& operator=(std::initializer_list<value_type> v)
    {
        this->container_ = std::move(v);
        return *this;
    }

    iterator       begin()        noexcept {return container_.begin();}
    iterator       end()          noexcept {return container_.end();}
    const_iterator begin()  const noexcept {return container_.begin();}
    const_iterator end()    const noexcept {return container_.end();}
    const_iterator cbegin() const noexcept {return container_.cbegin();}
    const_iterator cend()   const noexcept {return container_.cend();}

    bool        empty()    const noexcept {return container_.empty();}
    std::size_t size()     const noexcept {return container_.size();}
    std::size_t max_size() const noexcept {return container_.max_size();}

    void clear() {container_.clear();}

    void push_back(const value_type& v)
    {
        if(this->contains(v.first))
        {
            throw std::out_of_range("ordered_map: value already exists");
        }
        container_.push_back(v);
    }
    void push_back(value_type&& v)
    {
        if(this->contains(v.first))
        {
            throw std::out_of_range("ordered_map: value already exists");
        }
        container_.push_back(std::move(v));
    }
    void emplace_back(key_type k, mapped_type v)
    {
        if(this->contains(k))
        {
            throw std::out_of_range("ordered_map: value already exists");
        }
        container_.emplace_back(std::move(k), std::move(v));
    }
    void pop_back()  {container_.pop_back();}

    void insert(value_type kv)
    {
        if(this->contains(kv.first))
        {
            throw std::out_of_range("ordered_map: value already exists");
        }
        container_.push_back(std::move(kv));
    }
    void emplace(key_type k, mapped_type v)
    {
        if(this->contains(k))
        {
            throw std::out_of_range("ordered_map: value already exists");
        }
        container_.emplace_back(std::move(k), std::move(v));
    }

    std::size_t count(const key_type& key) const
    {
        if(this->find(key) != this->end())
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
    bool contains(const key_type& key) const
    {
        return this->find(key) != this->end();
    }
    iterator find(const key_type& key) noexcept
    {
        return std::find_if(this->begin(), this->end(),
            [&key, this](const value_type& v) {return this->cmp_(v.first, key);});
    }
    const_iterator find(const key_type& key) const noexcept
    {
        return std::find_if(this->begin(), this->end(),
            [&key, this](const value_type& v) {return this->cmp_(v.first, key);});
    }

    mapped_type&       at(const key_type& k)
    {
        const auto iter = this->find(k);
        if(iter == this->end())
        {
            throw std::out_of_range("ordered_map: no such element");
        }
        return iter->second;
    }
    mapped_type const& at(const key_type& k) const
    {
        const auto iter = this->find(k);
        if(iter == this->end())
        {
            throw std::out_of_range("ordered_map: no such element");
        }
        return iter->second;
    }

    mapped_type& operator[](const key_type& k)
    {
        const auto iter = this->find(k);
        if(iter == this->end())
        {
            this->container_.emplace_back(k, mapped_type{});
            return this->container_.back().second;
        }
        return iter->second;
    }

    mapped_type const& operator[](const key_type& k) const
    {
        const auto iter = this->find(k);
        if(iter == this->end())
        {
            throw std::out_of_range("ordered_map: no such element");
        }
        return iter->second;
    }

    key_compare key_comp() const {return this->cmp_;}

    void swap(ordered_map& other)
    {
        container_.swap(other.container_);
    }

  private:

    container_type container_;
};

template<typename K, typename V, typename C, typename A>
bool operator==(const ordered_map<K,V,C,A>& lhs, const ordered_map<K,V,C,A>& rhs)
{
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}
template<typename K, typename V, typename C, typename A>
bool operator!=(const ordered_map<K,V,C,A>& lhs, const ordered_map<K,V,C,A>& rhs)
{
    return !(lhs == rhs);
}
template<typename K, typename V, typename C, typename A>
bool operator<(const ordered_map<K,V,C,A>& lhs, const ordered_map<K,V,C,A>& rhs)
{
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}
template<typename K, typename V, typename C, typename A>
bool operator>(const ordered_map<K,V,C,A>& lhs, const ordered_map<K,V,C,A>& rhs)
{
    return rhs < lhs;
}
template<typename K, typename V, typename C, typename A>
bool operator<=(const ordered_map<K,V,C,A>& lhs, const ordered_map<K,V,C,A>& rhs)
{
    return !(lhs > rhs);
}
template<typename K, typename V, typename C, typename A>
bool operator>=(const ordered_map<K,V,C,A>& lhs, const ordered_map<K,V,C,A>& rhs)
{
    return !(lhs < rhs);
}

template<typename K, typename V, typename C, typename A>
void swap(ordered_map<K,V,C,A>& lhs, ordered_map<K,V,C,A>& rhs)
{
    lhs.swap(rhs);
    return;
}


} // toml
#endif // TOML11_ORDERED_MAP_HPP
