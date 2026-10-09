#pragma once

#include "seed/core/Types.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace seed {

class Scene {
public:
    EntityId create_entity(std::string name = "Entity");
    bool destroy_entity(EntityId entity);
    bool is_alive(EntityId entity) const;

    const std::string& entity_name(EntityId entity) const;
    void set_entity_name(EntityId entity, std::string name);

    std::size_t entity_count() const noexcept { return m_entities.size(); }

    template <typename T, typename... Args>
    T& add_component(EntityId entity, Args&&... args) {
        ensure_alive(entity);
        auto& pool = pool_for<T>();
        auto value = T{std::forward<Args>(args)...};
        auto [it, inserted] = pool.data.insert_or_assign(entity, std::move(value));
        (void)inserted;
        return it->second;
    }

    template <typename T>
    bool has_component(EntityId entity) const {
        const auto* pool = pool_if_exists<T>();
        return pool != nullptr && pool->data.contains(entity);
    }

    template <typename T>
    T* get_component(EntityId entity) {
        auto* pool = pool_if_exists<T>();
        if (pool == nullptr) {
            return nullptr;
        }
        const auto it = pool->data.find(entity);
        return it == pool->data.end() ? nullptr : &it->second;
    }

    template <typename T>
    const T* get_component(EntityId entity) const {
        const auto* pool = pool_if_exists<T>();
        if (pool == nullptr) {
            return nullptr;
        }
        const auto it = pool->data.find(entity);
        return it == pool->data.end() ? nullptr : &it->second;
    }

    template <typename T>
    bool remove_component(EntityId entity) {
        auto* pool = pool_if_exists<T>();
        return pool != nullptr && pool->data.erase(entity) > 0;
    }

    template <typename... Components, typename Function>
    void for_each(Function&& function) {
        for (auto& [entity, name] : m_entities) {
            (void)name;
            if ((has_component<Components>(entity) && ...)) {
                function(entity, *get_component<Components>(entity)...);
            }
        }
    }

    template <typename... Components, typename Function>
    void for_each(Function&& function) const {
        for (const auto& [entity, name] : m_entities) {
            (void)name;
            if ((has_component<Components>(entity) && ...)) {
                function(entity, *get_component<Components>(entity)...);
            }
        }
    }

private:
    struct IComponentPool {
        virtual ~IComponentPool() = default;
        virtual void erase(EntityId entity) = 0;
    };

    template <typename T>
    struct ComponentPool final : IComponentPool {
        std::unordered_map<EntityId, T> data;

        void erase(EntityId entity) override {
            data.erase(entity);
        }
    };

    template <typename T>
    ComponentPool<T>& pool_for() {
        const std::type_index key{typeid(T)};
        auto it = m_component_pools.find(key);
        if (it == m_component_pools.end()) {
            auto pool = std::make_unique<ComponentPool<T>>();
            auto* raw = pool.get();
            m_component_pools.emplace(key, std::move(pool));
            return *raw;
        }
        return *static_cast<ComponentPool<T>*>(it->second.get());
    }

    template <typename T>
    ComponentPool<T>* pool_if_exists() {
        const auto it = m_component_pools.find(std::type_index{typeid(T)});
        return it == m_component_pools.end()
            ? nullptr
            : static_cast<ComponentPool<T>*>(it->second.get());
    }

    template <typename T>
    const ComponentPool<T>* pool_if_exists() const {
        const auto it = m_component_pools.find(std::type_index{typeid(T)});
        return it == m_component_pools.end()
            ? nullptr
            : static_cast<const ComponentPool<T>*>(it->second.get());
    }

    void ensure_alive(EntityId entity) const;

    EntityId m_next_entity{1};
    std::unordered_map<EntityId, std::string> m_entities;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_component_pools;
};

} // namespace seed
