#ifndef UNIQUE_RESOURCE_HPP
#define UNIQUE_RESOURCE_HPP


template<typename T, typename Deleter>
class UniqueResource {
public:
    UniqueResource(T resource_)
        : resource{resource_}
        , deleter{}
        , invalid{}
    {}

    UniqueResource(T resource_, Deleter deleter_)
        : resource{resource_}
        , deleter{deleter_}
        , invalid{}
    {}

    UniqueResource(T resource_, Deleter deleter_, T invalid_)
        : resource(resource_)
        , deleter(deleter_)
        , invalid(invalid_)
    {}

    ~UniqueResource() {
        if (resource != invalid) {
            deleter(resource);
        }
    }

    UniqueResource(const UniqueResource&) = delete;
    UniqueResource& operator=(const UniqueResource&) = delete;

    UniqueResource(UniqueResource&& other) noexcept
        : resource(other.resource)
        , deleter(other.deleter)
        , invalid(other.invalid)
    {
        other.resource = invalid;
    }

    UniqueResource& operator=(UniqueResource&& other) noexcept
    {
        if (this != &other) {
            if (resource != invalid) {
                deleter(resource);
            }
            resource = other.resource;
            other.resource = invalid;
        }
        return *this;
    }

    T get() const
    {
        return resource;
    }

private:
    T resource;
    Deleter deleter;
    T invalid;
};


#endif  // UNIQUE_RESOURCE_HPP
