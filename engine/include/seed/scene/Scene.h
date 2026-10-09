#pragma once

#include "seed/core/Types.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace seed {

struct ParentComponent {
    std::string parent_persistent_id;
};

class Scene {
public:
    Scene() = default;
    Scene(const Scene& other);
    Scene& operator=(const Scene& other);
    Scene(Scene&&) noexcept = default;
    Scene& operator=(Scene&&) noexcept = default;
    ~Scene() = default;

    EntityId create_entity(std::string name = "Entity", std::string persistent_id = {});
    bool destroy_entity(EntityId entity);
    bool is_alive(EntityId entity) const;
    void clear();

    const std::string& entity_name(EntityId entity) const;
    void set_entity_name(EntityId entity, std::string name);
    const std::string& entity_persistent_id(EntityId entity) const;
    EntityId find_entity_by_persistent_id(const std::string& persistent_id) const noexcept;

    std::size_t entity_count() const noexcept { return m_entities.size(); }

    template <typename Function>
    void for_each_entity(Function&& function) {
        for (auto& [entity, record] : m_entities) function(entity, record.name);
    }

    template <typename Function>
    void for_each_entity(Function&& function) const {
        for (const auto& [entity, record] : m_entities) function(entity, record.name);
    }

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
        if (pool == nullptr) return nullptr;
        const auto it = pool->data.find(entity);
        return it == pool->data.end() ? nullptr : &it->second;
    }

    template <typename T>
    const T* get_component(EntityId entity) const {
        const auto* pool = pool_if_exists<T>();
        if (pool == nullptr) return nullptr;
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
        for (auto& [entity, record] : m_entities) {
            (void)record;
            if ((has_component<Components>(entity) && ...)) function(entity, *get_component<Components>(entity)...);
        }
    }

    template <typename... Components, typename Function>
    void for_each(Function&& function) const {
        for (const auto& [entity, record] : m_entities) {
            (void)record;
            if ((has_component<Components>(entity) && ...)) function(entity, *get_component<Components>(entity)...);
        }
    }

private:
    struct EntityRecord {
        std::string name;
        std::string persistent_id;
    };

    struct IComponentPool {
        virtual ~IComponentPool() = default;
        virtual void erase(EntityId entity) = 0;
        virtual std::unique_ptr<IComponentPool> clone() const = 0;
    };

    template <typename T>
    struct ComponentPool final : IComponentPool {
        std::unordered_map<EntityId, T> data;
        void erase(EntityId entity) override { data.erase(entity); }
        std::unique_ptr<IComponentPool> clone() const override { return std::make_unique<ComponentPool<T>>(*this); }
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
        return it == m_component_pools.end() ? nullptr : static_cast<ComponentPool<T>*>(it->second.get());
    }

    template <typename T>
    const ComponentPool<T>* pool_if_exists() const {
        const auto it = m_component_pools.find(std::type_index{typeid(T)});
        return it == m_component_pools.end() ? nullptr : static_cast<const ComponentPool<T>*>(it->second.get());
    }

    void ensure_alive(EntityId entity) const;
    void copy_from(const Scene& other);
    static std::string generate_persistent_id();

    EntityId m_next_entity{1};
    std::unordered_map<EntityId, EntityRecord> m_entities;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_component_pools;
};

} // namespace seed
