/*
 * Clay Texture Manager
 *
 * Loads and caches Irrlicht textures for use in Clay GUI layouts.
 *
 * Part of the Luantis Clay GUI integration (v9.49).
 */

#include "clay_texture_manager.h"

#include "client/texturesource.h"
#include "log.h"

void ClayTextureManager::setTextureSource(ISimpleTextureSource *tsrc)
{
	m_tsrc = tsrc;
}

video::ITexture *ClayTextureManager::get(const char *name)
{
	if (!name || !name[0])
		return nullptr;

	// Check cache first
	auto it = m_cache.find(name);
	if (it != m_cache.end())
		return it->second;

	// Load via TextureSource
	video::ITexture *tex = nullptr;
	if (m_tsrc) {
		tex = m_tsrc->getTexture(name);
	}

	if (tex) {
		m_cache[name] = tex;
		infostream << "ClayTextureManager: loaded texture '" << name << "'" << std::endl;
	} else {
		m_cache[name] = nullptr; // Cache the miss to avoid repeated lookups
		warningstream << "ClayTextureManager: failed to load texture '" << name << "'" << std::endl;
	}

	return tex;
}

video::ITexture *ClayTextureManager::get(const std::string &name)
{
	return get(name.c_str());
}

int ClayTextureManager::preload(const char *names[], int count)
{
	int loaded = 0;
	for (int i = 0; i < count; i++) {
		if (get(names[i]))
			loaded++;
	}
	infostream << "ClayTextureManager: preloaded " << loaded << "/" << count << " textures" << std::endl;
	return loaded;
}

void ClayTextureManager::clearCache()
{
	m_cache.clear();
	infostream << "ClayTextureManager: cache cleared" << std::endl;
}
