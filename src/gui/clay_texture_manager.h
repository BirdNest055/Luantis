/*
 * Clay Texture Manager
 *
 * Loads and caches Irrlicht textures for use in Clay GUI layouts.
 * Provides a simple name-based lookup: texture names from Theme::Textures
 * and Theme::Icons are resolved to video::ITexture* pointers that can
 * be passed as Clay imageData.
 *
 * Usage in Clay layouts:
 *   auto *tex = ClayTextureManager::get(Theme::Icons::settings);
 *   CLAY(CLAY_ID("MyIcon"), Theme::Styles::iconImage()) {
 *       CLAY_IMAGE(..., { .imageData = tex });
 *   }
 *
 * Part of the Luantis Clay GUI integration (v9.49).
 */

#pragma once

#include <string>
#include <unordered_map>

namespace video {
class ITexture;
}

class ISimpleTextureSource;

class ClayTextureManager {
public:
	ClayTextureManager() = default;
	~ClayTextureManager() = default;

	/** Set the texture source for loading textures by name. */
	void setTextureSource(ISimpleTextureSource *tsrc);

	/**
	 * Get a texture by name (e.g., Theme::Icons::settings = "settings_btn").
	 * Returns the cached ITexture* or loads it via TextureSource on first access.
	 * Returns nullptr if the texture cannot be loaded.
	 */
	video::ITexture *get(const char *name);

	/**
	 * Get a texture by std::string.
	 * Same as get(const char*) but accepts std::string.
	 */
	video::ITexture *get(const std::string &name);

	/**
	 * Preload a set of textures by name.
	 * Call this during init to avoid hitches during first render.
	 * Returns the number of textures successfully loaded.
	 */
	int preload(const char *names[], int count);

	/** Clear the texture cache. Call when the texture source changes. */
	void clearCache();

	/** Get the number of cached textures. */
	size_t cacheSize() const { return m_cache.size(); }

private:
	ISimpleTextureSource *m_tsrc = nullptr;
	std::unordered_map<std::string, video::ITexture *> m_cache;
};
