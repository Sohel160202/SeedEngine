#include "seed/scene/Scene.h"

#include <array>
#include <iomanip>
#include <random>
#include <sstream>

namespace seed {

Scene::Scene(const Scene& other) {
    copy_from(other);
}

Scene& Scene::operator=(const Scene& other) {
    if (this != &other) {
        copy_from(other);
    }
    return *this;
}

void Scene::copy_from(const Scene& other) {
    m_next_entity = other.m_next_entity;
    m_entities = other.m_entities;
    m_component_pools.clear();
    for (const auto& [type, pool] : other.m_component_pools) {
        m_component_pools.emplace(type, pool->clone());
    }
}

EntityId Scene::create_entity(std::string name, std::string persistent_id) {
    if (persistent_id.empty()) {
        persistent_id = generate_persistent_id();
    } else {
        for (const auto& [_, record] : m_entities) {
            if (record.persistent_id == persistent_id) {
                throw std::runtime_error("Seed Scene: duplicate persistent entity id");
            }
        }
    }

    const EntityId entity = m_next_entity++;
    m_entities.emplace(entity, EntityRecord{
        .name = std::move(name),
        .persistent_id = std::move(persistent_id),
    });
    return entity;
}

bool Scene::destroy_entity(EntityId entity) {
    if (m_entities.erase(entity) == 0) {
        return false;
    }

    for (auto& [_, pool] : m_component_pools) {
        pool->erase(entity);
    }
    return true;
}

bool Scene::is_alive(EntityId entity) const {
    return m_entities.contains(entity);
}

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

void Scene::ensure_alive(EntityId entity) const {
    if (!is_alive(entity)) {
        throw std::runtime_error("Seed Scene: entity is not alive");
    }
}

std::string Scene::generate_persistent_id() {
    static std::random_device random_device;
    static std::mt19937 generator(random_device());
    static std::uniform_int_distribution<int> distribution(0, 255);

    std::array<unsigned char, 16> bytes{};
    for (auto& byte : bytes) {
        byte = static_cast<unsigned char>(distribution(generator));
    }

    // RFC 4122 style version 4 / variant bits. Seed only relies on stability,
    // but using a familiar UUID representation makes scene files readable.
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        stream << std::setw(2) << static_cast<int>(bytes[index]);
        if (index == 3 || index == 5 || index == 7 || index == 9) {
            stream << '-';
        }
    }
    return stream.str();
}

} // namespace seed
