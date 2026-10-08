#include "seed/scene/Scene.h"

namespace seed {

EntityId Scene::create_entity(std::string name) {
    const EntityId entity = m_next_entity++;
    m_entities.emplace(entity, std::move(name));
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

const std::string& Scene::entity_name(EntityId entity) const {
    ensure_alive(entity);
    return m_entities.at(entity);
}

void Scene::set_entity_name(EntityId entity, std::string name) {
    ensure_alive(entity);
    m_entities.at(entity) = std::move(name);
}

void Scene::ensure_alive(EntityId entity) const {
    if (!is_alive(entity)) {
        throw std::runtime_error("Seed Scene: entity is not alive");
    }
}

} // namespace seed
