#include "seed/scene/Scene.h"

#include <array>
#include <iomanip>
#include <random>
#include <sstream>

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
