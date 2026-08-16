#pragma once

#include "ankerl/ankerl.h"
#include "d3/OutrageModel.h"
#include "Graphics/GraphicsHandles.h"
#include "Handles.h"
#include "neon-types.h"
#include "Utility.h"

namespace neon {

struct ModelEntry {
    d3::Model model;
    MeshID mesh;
};

// We are using the D3 model definition as the standard.
// D1/D2 models can be loaded into it by transforming animations to the new format.
class ModelCache {
    List<ModelEntry> _models;
    CaseInsensitiveDictionary<ModelID> _lookup;

public:
    ModelID Add(const d3::Model& model, string_view name, MeshID meshId) {
        auto id = (ModelID)_models.size();
        // _models.push_back(model);
        _models.push_back({
            .model = model,
            .mesh = meshId
        });
        _lookup[name] = id;
        return id;
    }

    bool Contains(string_view name) const {
        return _lookup.contains(name);
    }

    ModelID Find(string_view name) {
        if (_lookup.contains(name)) return _lookup[name];
        return ModelID::None;
    }

    ModelEntry* Get(ModelID model) {
        return Seq::tryItem(_models, (int)model);
    }

    ModelEntry* Get(string_view name) {
        return Get(Find(name));
    }
};

extern ModelCache g_ModelCache;

}
