#pragma once
#include <nlohmann/json.hpp>
#include "Genetics.h"
#include "Organism.h"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

// Helper to safely load data. If the key is missing (from an older save file), 
// it leaves the variable with its default C++ initialization.
template <typename T>
void safe_get(const json& j, const std::string& key, T& val) {
    if (j.contains(key) && !j[key].is_null()) {
        val = j[key].get<T>();
    }
}

namespace nlohmann {

    // --- MorphologyGene ---
    template <>
    struct adl_serializer<MorphologyGene> {
        static void to_json(json& j, const MorphologyGene& m) {
            j = json{{"type", static_cast<int>(m.type)}, {"p1", m.p1_geneIndex}, {"p2", m.p2_geneIndex}, 
                     {"len", m.length}, {"ang", m.branchAngle}, {"musc", m.isMuscle}, 
                     {"ioId", m.ioNeuronId}, {"sens", m.sensorRange}};
        }
        static void from_json(const json& j, MorphologyGene& m) {
            int typeInt = 0;
            safe_get(j, "type", typeInt); m.type = static_cast<ColorType>(typeInt);
            safe_get(j, "p1", m.p1_geneIndex);
            safe_get(j, "p2", m.p2_geneIndex);
            safe_get(j, "len", m.length);
            safe_get(j, "ang", m.branchAngle);
            safe_get(j, "musc", m.isMuscle);
            safe_get(j, "ioId", m.ioNeuronId);
            safe_get(j, "sens", m.sensorRange);
        }
    };

    // --- NeuronGene ---
    template <>
    struct adl_serializer<NeuronGene> {
        static void to_json(json& j, const NeuronGene& n) {
            j = json{{"id", n.id}, {"role", static_cast<int>(n.role)}, {"pol", static_cast<int>(n.polarity)},
                     {"thresh", n.threshold}, {"leak", n.leakRate}, {"rest", n.restPotential}, {"x", n.x}, {"y", n.y}};
        }
        static void from_json(const json& j, NeuronGene& n) {
            safe_get(j, "id", n.id);
            int roleInt = 0, polInt = 0;
            safe_get(j, "role", roleInt); n.role = static_cast<NeuronRole>(roleInt);
            safe_get(j, "pol", polInt); n.polarity = static_cast<NeuronPolarity>(polInt);
            safe_get(j, "thresh", n.threshold);
            safe_get(j, "leak", n.leakRate);
            safe_get(j, "rest", n.restPotential);
            safe_get(j, "x", n.x);
            safe_get(j, "y", n.y);
        }
    };

    // --- SynapseGene ---
    template <>
    struct adl_serializer<SynapseGene> {
        static void to_json(json& j, const SynapseGene& s) {
            j = json{{"src", s.sourceId}, {"tgt", s.targetId}, {"w", s.weight}};
        }
        static void from_json(const json& j, SynapseGene& s) {
            safe_get(j, "src", s.sourceId);
            safe_get(j, "tgt", s.targetId);
            safe_get(j, "w", s.weight);
        }
    };

    // --- Genome ---
    template <>
    struct adl_serializer<Genome> {
        static void to_json(json& j, const Genome& g) {
            j = json{{"morph", g.morphology}, {"neurons", g.neurons}, 
                     {"synapses", g.synapses}, {"life", g.lifespan}, {"sym", g.symmetry}};
        }
        static void from_json(const json& j, Genome& g) {
            safe_get(j, "morph", g.morphology);
            safe_get(j, "neurons", g.neurons);
            safe_get(j, "synapses", g.synapses);
            safe_get(j, "life", g.lifespan);
            safe_get(j, "sym", g.symmetry);
        }
    };
}