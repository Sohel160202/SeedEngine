#include "seed/scene/Scene.h"

#include <array>
#include <iomanip>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace seed {

Scene::Scene(const Scene& other) { copy_from(other); }

Scene& Scene::operator=(const Scene& other) {
    if (this != &other) copy_from(other);
    return *this;
}

void Scene::copy_from(const Scene& other) {
    m_next_entity = other.m_next_entity;
    m_entities = other.m_entities;
    m_component_pools.clear();
    for (const auto& [type, pool] : other.m_component_pools) m_component_pools.emplace(type, pool->clone());
}

EntityId Scene::create_entity(std::string name, std::string persistent_id) {
    if (persistent_id.empty()) {
        persistent_id = generate_persistent_id();
    } else if (find_entity_by_persistent_id(persistent_id) != InvalidEntity) {
        throw std::runtime_error("Seed Scene: duplicate persistent entity id");
    }

    const EntityId entity = m_next_entity++;
    m_entities.emplace(entity, EntityRecord{.name = std::move(name), .persistent_id = std::move(persistent_id)});
    return entity;
}

std::vector<EntityId> Scene::copy_hierarchy_from(
    const Scene& source,
    EntityId source_root,
    std::string root_name,
    std::string persistent_id
) {
    source.ensure_alive(source_root);
    if (root_name.empty()) root_name = source.entity_name(source_root);

    std::vector<EntityId> source_entities{source_root};
    for (std::size_t index = 0; index < source_entities.size(); ++index) {
        const EntityId parent = source_entities[index];
        const std::string& parent_id = source.entity_persistent_id(parent);
        source.for_each_entity([&](EntityId candidate, const std::string&) {
            for (const EntityId existing : source_entities) {
                if (existing == candidate) return;
            }
            const auto* relation = source.get_component<ParentComponent>(candidate);
            if (relation != nullptr && relation->parent_persistent_id == parent_id) {
                source_entities.push_back(candidate);
            }
        });
    }

    std::unordered_map<std::string, EntityId> entity_map;
    std::vector<EntityId> destination_entities;
    destination_entities.reserve(source_entities.size());
    for (std::size_t index = 0; index < source_entities.size(); ++index) {
        const EntityId source_entity = source_entities[index];
        const std::string name = index == 0 ? root_name : source.entity_name(source_entity);
        const EntityId destination_entity = create_entity(
            name,
            index == 0 ? std::move(persistent_id) : std::string{}
        );
        entity_map.emplace(source.entity_persistent_id(source_entity), destination_entity);
        destination_entities.push_back(destination_entity);
    }

    for (std::size_t index = 0; index < source_entities.size(); ++index) {
        const EntityId source_entity = source_entities[index];
        const EntityId destination_entity = destination_entities[index];
        for (const auto& [type, pool] : source.m_component_pools) {
            (void)type;
            pool->copy_entity_to(source_entity, *this, destination_entity);
        }
    }

    for (std::size_t index = 0; index < source_entities.size(); ++index) {
        const EntityId destination_entity = destination_entities[index];
        auto* relation = get_component<ParentComponent>(destination_entity);
        if (relation == nullptr) continue;
        if (index == 0) {
            relation->parent_persistent_id.clear();
            continue;
        }
        const auto mapped = entity_map.find(relation->parent_persistent_id);
        relation->parent_persistent_id =
            mapped == entity_map.end()
                ? std::string{}
                : entity_persistent_id(mapped->second);
    }
    return destination_entities;
}

EntityId Scene::copy_entity_from(
    const Scene& source,
    EntityId source_entity,
    std::string name,
    std::string persistent_id
) {
    source.ensure_alive(source_entity);
    if (name.empty()) name = source.entity_name(source_entity);
    const EntityId destination_entity = create_entity(
        std::move(name),
        std::move(persistent_id)
    );
    for (const auto& [type, pool] : source.m_component_pools) {
        (void)type;
        pool->copy_entity_to(source_entity, *this, destination_entity);
    }
    // A single-entity prefab is instantiated as a root. Hierarchy-aware
    // prefabs will preserve parent/child relationships in a later format version.
    if (auto* parent = get_component<ParentComponent>(destination_entity)) {
        parent->parent_persistent_id.clear();
    }
    return destination_entity;
}

bool Scene::destroy_entity(EntityId entity) {
    const auto found = m_entities.find(entity);
    if (found == m_entities.end()) return false;
    const std::string persistent_id = found->second.persistent_id;

    // Children become roots when their parent is deleted. Keeping this cleanup in
    // Scene prevents stale hierarchy references in editor and runtime code.
    if (auto* parents = pool_if_exists<ParentComponent>()) {
        for (auto& [child, parent] : parents->data) {
            (void)child;
            if (parent.parent_persistent_id == persistent_id) parent.parent_persistent_id.clear();
        }
    }

    m_entities.erase(found);
    for (auto& [_, pool] : m_component_pools) pool->erase(entity);
    return true;
}

bool Scene::is_alive(EntityId entity) const { return m_entities.contains(entity); }

void Scene::clear() {
    m_component_pools.clear();
    m_entities.clear();
    m_next_entity = 1;
}

const std::string& Scene::entity_name(EntityId entity) const {
    ensure_alive(entity);
    return m_entities.at(entity).name;
}

void Scene::set_entity_name(EntityId entity, std::string name) {
    ensure_alive(entity);
    m_entities.at(entity).name = std::move(name);
}

const std::string& Scene::entity_persistent_id(EntityId entity) const {
    ensure_alive(entity);
    return m_entities.at(entity).persistent_id;
}

EntityId Scene::find_entity_by_persistent_id(const std::string& persistent_id) const noexcept {
    if (persistent_id.empty()) return InvalidEntity;
    for (const auto& [entity, record] : m_entities) {
        if (record.persistent_id == persistent_id) return entity;
    }
    return InvalidEntity;
}

void Scene::ensure_alive(EntityId entity) const {
    if (!is_alive(entity)) throw std::runtime_error("Seed Scene: entity is not alive");
}

std::string Scene::generate_persistent_id() {
    static std::random_device random_device;
    static std::mt19937 generator(random_device());
    static std::uniform_int_distribution<int> distribution(0, 255);

    std::array<unsigned char, 16> bytes{};
    for (auto& byte : bytes) byte = static_cast<unsigned char>(distribution(generator));
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        stream << std::setw(2) << static_cast<int>(bytes[index]);
        if (index == 3 || index == 5 || index == 7 || index == 9) stream << '-';
    }
    return stream.str();
}

} // namespace seed
