#pragma once
#include "thru_composer_json.h"
#include "thru_fm_parameters.h"
#include "thru_source_motion.h"
#include "thru_lowform_parameters.h"
#include "thru_midi_parameters.h"
#include "thru_noise_parameters.h"
#include "thru_partials_parameters.h"
#include "thru_reson_parameters.h"
#include "thru_scanned_parameters.h"
#include "thru_sequence_lock_catalog.h"
#include "thru_table_parameters.h"
#include <set>
namespace s3g_thru_pack {
using J = s3g_thru_json::Value;
inline constexpr const char *engines[]{
    "motion",  "lanes",      "grains",   "cutups", "sample",  "slices",
    "glitch",  "weave",      "wavesets", "prism",  "morph",   "texture",
    "formant", "stochastic", "pulsar",   "table",  "scanned", "fm",
    "noise",   "reson",      "partials", "lowform"};
inline constexpr const char *modes[]{"single", "split",  "shared-dual", "quad",
                                     "bank",   "layers", "sequence"};
inline constexpr const char *pointTypes[]{
    "parameter",         "pressure",          "stack-position",
    "stack-hold",        "roll-position",     "roll-lane-position",
    "part-parameter",    "part-hold",         "performance",
    "stack-path",        "note-pressure",     "midi-expression",
    "layers-parameter",  "layers-baseline",   "sequence-lane",
    "sequence-recipe",   "control-lfo",       "pooled-lfo",
    "table-parameter",   "scanned-parameter", "fm-parameter",
    "noise-parameter",   "reson-parameter",   "partials-parameter",
    "lowform-parameter", "reson-density", "source-parameter"};
inline constexpr const char *rates[]{"1/1",   "1/2",  "1/4",   "1/8", "1/16",
                                     "1/16T", "1/32", "1/32T", "1/64"};
inline constexpr const char *directions[]{"forward", "backward", "ping-pong",
                                          "repeat-ends", "random"};
inline constexpr const char *paths[]{"horizontal", "vertical", "diagonal"};
inline bool safeId(const std::string &id) {
  if (id.empty() || id.size() > 64 || id == "__proto__" ||
      id == "constructor" || id == "prototype")
    return false;
  for (unsigned char c : id)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_'))
      return false;
  return true;
}
inline bool hashId(const std::string &id) {
  return id.size() == 64 &&
         id.find_first_not_of("0123456789abcdef") == std::string::npos;
}
inline bool label(const std::string &s) {
  if (s.size() > 40)
    return false;
  for (unsigned char c : s)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_'))
      return false;
  return true;
}
inline void need(bool ok, const std::string &where, const char *why) {
  if (!ok)
    throw std::runtime_error(where + ": " + why);
}
inline void object(const J &v, const std::string &where) {
  need(v.isObject(), where, "expected object");
}
inline void keys(const J &v, const std::string &where,
                 std::initializer_list<const char *> allowed) {
  object(v, where);
  for (const auto &[k, x] : v.objectValue()) {
    (void)x;
    bool known = false;
    for (auto a : allowed)
      known |= k == a;
    need(known, where + "." + k, "unknown field");
  }
}
inline const J &field(const J &v, const char *key, std::string_view where) {
  object(v, std::string(where));
  const auto *f = v.find(key);
  need(f, std::string(where) + "." + key, "required field");
  return *f;
}
inline double number(const J &v, const std::string &where, double low,
                     double high, bool integer = false) {
  need(v.isNumber() && std::isfinite(v.number()) && v.number() >= low &&
           v.number() <= high &&
           (!integer || std::floor(v.number()) == v.number()),
       where, "number outside supported range");
  return v.number();
}
inline std::string string(const J &v, std::string where,
                          std::size_t max = 120) {
  need(v.isString() && v.string().size() <= max &&
           v.string().find('\0') == std::string::npos,
       where, "invalid text");
  return v.string();
}
inline bool boolean(const J &v, const std::string &where) {
  need(v.isBool(), where, "expected boolean");
  return v.boolean();
}
struct ArrayView {
  const J::Array *values;
  auto begin() const { return values->begin(); }
  auto end() const { return values->end(); }
  std::size_t size() const { return values->size(); }
  const J &operator[](std::size_t i) const { return (*values)[i]; }
};
inline ArrayView array(const J &v, std::string_view where, std::size_t max,
                       std::size_t minimum = 0) {
  need(v.isArray() && v.arrayValue().size() >= minimum &&
           v.arrayValue().size() <= max,
       std::string(where), "array length outside supported range");
  return {&v.arrayValue()};
}
template <std::size_t N>
int choice(const J &v, const std::string &where,
           const char *const (&values)[N]) {
  const auto &s = string(v, where);
  for (std::size_t i = 0; i < N; ++i)
    if (s == values[i])
      return int(i);
  throw std::runtime_error(where + ": unsupported choice " + s);
}
inline double optNumber(const J &v, const char *key, const std::string &w,
                        double fallback, double low, double high,
                        bool integer = false) {
  const auto *n = v.find(key);
  return n ? number(*n, w + "." + key, low, high, integer) : fallback;
}
inline bool optBool(const J &v, const char *key, const std::string &w,
                    bool fallback) {
  const auto *n = v.find(key);
  return n ? boolean(*n, w + "." + key) : fallback;
}
inline int optChoice(const J &v, const char *key, const std::string &w,
                     int fallback, const char *const *choices, int count) {
  if (const auto *x = v.find(key)) {
    const auto &s = string(*x, w + "." + key);
    for (int i = 0; i < count; ++i)
      if (s == choices[i])
        return i;
    throw std::runtime_error(w + "." + key + ": unsupported choice " + s);
  }
  return fallback;
}
inline const s3g_thru_midi::Parameter *parameter(const std::string &key) {
  for (const auto &p : s3g_thru_midi::parameters)
    if (key == p.key)
      return &p;
  return nullptr;
}
inline bool programParameter(const s3g_thru_midi::Parameter &p) {
  return !p.action &&
         ((p.id >= 4200 && p.id < 4200 + s3g_thru_table::fields) ||
          (p.id >= 4300 && p.id < 4300 + s3g_thru_scanned::fields) ||
          (p.id >= 4400 && p.id < 4400 + s3g_thru_fm::fields) ||
          (p.id >= 4500 && p.id < 4500 + s3g_thru_noise::fields) ||
          (p.id >= 4600 && p.id < 4600 + s3g_thru_reson::fields) ||
          (p.id >= 4800 && p.id < 4800 + s3g_thru_partials::fields) ||
          (p.id >= 5000 && p.id < 5000 + s3g_thru_lowform::fields));
}
inline bool soundParameter(const s3g_thru_midi::Parameter &p) {
  return !p.action && (p.lock >= 0 || programParameter(p) || p.id == 5091 || (p.id>=5200&&p.id<5213));
}
inline double parameterBound(float x) {
  if (x == 0)
    return 0;
  const double scale =
      std::pow(10.0, 6 - std::floor(std::log10(std::abs(double(x)))));
  return std::round(double(x) * scale) / scale;
}
inline void parameters(const J &v, const std::string &where) {
  object(v, where);
  need(v.objectValue().size() <= 943, where, "too many parameters");
  for (const auto &[key, x] : v.objectValue()) {
    const auto *p = parameter(key);
    need(p && soundParameter(*p), where + "." + key,
         "unsupported sound parameter");
    number(x, where + "." + key, parameterBound(p->minimum),
           parameterBound(p->maximum), p->integer);
  }
}
inline void duration(const J &v, const std::string &where) {
  keys(v, where, {"unit", "value"});
  const auto &unit = string(field(v, "unit", where), where + ".unit");
  need(unit == "ms" || unit == "steps", where + ".unit",
       "expected ms or steps");
  number(field(v, "value", where), where + ".value", unit == "ms" ? 10 : .01,
         unit == "ms" ? 30000 : 32);
}
inline void locks(const J &v, const std::string &where) {
  keys(v, where, {"position", "pitch", "velocity", "duration", "sound"});
  if (const auto *x = v.find("position"))
    number(*x, where + ".position", 0, 1);
  if (const auto *x = v.find("pitch"))
    number(*x, where + ".pitch", -48, 48, true);
  if (const auto *x = v.find("velocity"))
    number(*x, where + ".velocity", 1.0 / 127, 1);
  if (const auto *x = v.find("duration"))
    duration(*x, where + ".duration");
  if (const auto *x = v.find("sound")) {
    parameters(*x, where + ".sound");
    for (const auto &[key, value] : x->objectValue())
      need(s3g_thru_sequence::soundDescriptor(parameter(key)->id),
           where + ".sound." + key, "parameter cannot be locked per trig");
  }
}
inline void trig(const J &v, const std::string &where, bool indexed = true) {
  keys(v, where,
       {"step", "enabled", "microtime", "probability", "cycle", "locks",
        "burst", "protected"});
  if (indexed)
    number(field(v, "step", where), where + ".step", 0, 31, true);
  if (const auto *x = v.find("enabled"))
    boolean(*x, where + ".enabled");
  if (const auto *x = v.find("microtime"))
    number(*x, where + ".microtime", -.5, .5);
  if (const auto *x = v.find("probability"))
    number(*x, where + ".probability", 0, 1);
  if (const auto *x = v.find("protected"))
    boolean(*x, where + ".protected");
  if (const auto *x = v.find("cycle")) {
    keys(*x, where + ".cycle", {"hit", "every"});
    const auto every =
        number(field(*x, "every", where), where + ".cycle.every", 1, 8, true);
    number(field(*x, "hit", where), where + ".cycle.hit", 1, every, true);
  }
  if (const auto *x = v.find("locks"))
    locks(*x, where + ".locks");
  if (const auto *x = v.find("burst")) {
    for (const auto &child : array(*x, where + ".burst", 8, 1)) {
      keys(child, where + ".burst", {"offset", "locks"});
      need(number(field(child, "offset", where), where + ".burst.offset", 0,
                  1) < 1,
           where + ".burst.offset", "must be less than one step");
      if (const auto *l = child.find("locks"))
        locks(*l, where + ".burst.locks");
    }
  }
}
inline void matchingUnit(const J &t, const std::string &unit,
                         const std::string &where) {
  const auto check = [&](const J &v) {
    if (const auto *l = v.find("locks"))
      if (const auto *d = l->find("duration"))
        need(d->at("unit").string() == unit, where,
             "duration unit must match the lane");
  };
  check(t);
  if (const auto *b = t.find("burst"))
    for (const auto &c : b->arrayValue())
      check(c);
}
inline void track(const J &v, const std::string &where) {
  keys(v, where,
       {"length", "rate", "rotation", "direction", "path", "seed", "enabled",
        "scaleEntry", "defaults", "trigs", "generator"});
  number(field(v, "length", where), where + ".length", 1, 32, true);
  choice(field(v, "rate", where), where + ".rate", rates);
  optNumber(v, "rotation", where, 0, 0, 31, true);
  optNumber(v, "seed", where, 0, 0, 4294967295.0, true);
  optChoice(v, "direction", where, 0, directions, 5);
  optChoice(v, "path", where, 0, paths, 3);
  optBool(v, "enabled", where, true);
  optBool(v, "scaleEntry", where, false);
  if (const auto *x = v.find("defaults")) {
    locks(*x, where + ".defaults");
    if (const auto *pitch = x->find("pitch"))
      number(*pitch, where + ".defaults.pitch", 0, 0);
    if (const auto *sound = x->find("sound"))
      need(sound->objectValue().empty(), where + ".defaults.sound",
           "put lane sound defaults in part.parameters");
  }
  std::string unit = "ms";
  if (const auto *d = v.find("defaults"))
    if (const auto *t = d->find("duration"))
      unit = t->at("unit").string();
  std::set<int> seen;
  for (const auto &x : array(field(v, "trigs", where), where + ".trigs", 32)) {
    trig(x, where + ".trig");
    matchingUnit(x, unit, where + ".trig");
    need(seen.insert(int(x.at("step").number())).second, where + ".trigs",
         "duplicate step");
  }
  if (const auto *x = v.find("generator")) {
    keys(*x, where + ".generator",
         {"type", "shift", "ready", "timed", "values", "base", "enabledMask",
          "enabledValues", "burstMask"});
    const auto &type = string(field(*x, "type", where), where);
    need(type == "normal" || type == "euclidean" || type == "onset", where,
         "unsupported generator");
    optNumber(*x, "shift", where, 0, 0, 31, true);
    optBool(*x, "ready", where, false);
    optBool(*x, "timed", where, false);
    for (auto k : {"enabledMask", "enabledValues", "burstMask"})
      optNumber(*x, k, where, 0, 0, 4294967295.0, true);
    if (const auto *vals = x->find("values")) {
      constexpr double low[]{0, 0, 0, 0, 1, 0, 0,   0,   1, -1, 0,
                             0, 0, 0, 0, 0, 0, .01, .01, 0, -1};
      constexpr double high[]{2, 32, 5,  6,  16, 1, 1,     65535, 64,  30, 1,
                              1, 5,  24, 32, 1,  3, 30000, 30000, .25, 1};
      const auto &a = array(*vals, where + ".generator.values", 21, 21);
      for (int i = 0; i < 21; ++i)
        number(a[i], where, low[i], high[i]);
    }
    if (const auto *base = x->find("base"))
      for (const auto &b : array(*base, where + ".generator.base", 32, 32)) {
        trig(b, where + ".generator.base", false);
        matchingUnit(b, optBool(*x, "timed", where, false) ? "ms" : "steps",
                     where + ".generator.base");
      }
  }
}
inline void validate(const J &doc) {
  keys(doc, "composition",
       {"$schema", "format", "version", "title", "tempo", "clips", "chains",
        "assets", "extensions", "provenance"});
  need(string(field(doc, "format", "composition"), "format") == "s3g-thrupack",
       "format", "unsupported format");
  number(field(doc, "version", "composition"), "version", 1, 1, true);
  need(label(string(field(doc, "title", "composition"), "title", 40)), "title",
       "use up to 40 letters, digits, spaces, hyphens or underscores");
  number(field(doc, "tempo", "composition"), "tempo", 1, 999);
  std::set<std::string> assets, clips;
  std::size_t noteCount = 0, pointCount = 0;
  for (const auto &a :
       array(field(doc, "assets", "composition"), "assets", 512)) {
    keys(a, "asset",
         {"id", "name", "sha256", "size", "file", "hint", "url", "waveform",
          "frames", "sampleRate"});
    const auto &id = string(field(a, "id", "asset"), "asset.id", 64);
    need(safeId(id) && assets.insert(id).second, "assets",
         "invalid or duplicate ID");
    string(field(a, "name", "asset"), "asset.name", 200);
    if (const auto *h = a.find("sha256"))
      need(hashId(string(*h, "asset.sha256", 64)), "asset.sha256",
           "expected lowercase SHA-256");
    if (const auto *n = a.find("size"))
      number(*n, "asset.size", 1, 512.0 * 1024 * 1024, true);
    if (const auto *f = a.find("file")) {
      need(a.find("sha256"), "asset.file", "hash required");
      need(string(*f, "asset.file") ==
               "assets/" + a.at("sha256").string() + ".wav",
           "asset.file", "invalid asset path");
    }
    if (const auto *hint = a.find("hint"))
      string(*hint, "asset.hint", 200);
    if (const auto *u = a.find("url")) {
      const auto &path = string(*u, "asset.url", 768);
      need(path.rfind("/files/download?path=", 0) == 0, "asset.url",
           "only local file downloads are allowed");
    }
    if (const auto *w = a.find("waveform"))
      for (const auto &n : array(*w, "asset.waveform", 256))
        number(n, "asset.waveform", -1, 1);
    if (const auto *n = a.find("frames"))
      number(*n, "asset.frames", 1, 2147483647, true);
    if (const auto *n = a.find("sampleRate"))
      number(*n, "asset.sampleRate", 8000, 384000);
  }
  for (const auto &c :
       array(field(doc, "clips", "composition"), "clips", 64, 1)) {
    keys(c, "clip",
         {"id", "name", "mode", "sources", "parts", "parameters", "sequence",
          "notes", "automation", "loop", "extensions"});
    const auto &id = string(field(c, "id", "clip"), "clip.id", 64);
    need(safeId(id) && clips.insert(id).second, "clips",
         "invalid or duplicate ID");
    need(label(string(field(c, "name", "clip"), "clip.name", 40)), "clip.name",
         "invalid saved name");
    const int mode = choice(field(c, "mode", "clip"), "clip.mode", modes);
    const int parts =
        (mode == 0 || mode == 5) ? 1 : (mode == 3 || mode == 6) ? 4 : 2;
    const auto &sources =
        array(field(c, "sources", "clip"), "clip.sources", 4, 4);
    for (const auto &source : sources) {
      keys(source, "source", {"asset", "label"});
      if (const auto *a = source.find("asset"); a && !a->isNull())
        need(assets.count(string(*a, "source.asset", 64)), "source.asset",
             "unknown asset");
      if (const auto *l = source.find("label"))
        string(*l, "source.label", 63);
    }
    const auto &partArray =
        array(field(c, "parts", "clip"), "clip.parts", parts, parts);
    for (const auto &p : partArray) {
      keys(p, "part", {"engine", "root", "scale", "parameters"});
      choice(field(p, "engine", "part"), "part.engine", engines);
      optNumber(p, "root", "part", 60, 0, 127, true);
      optNumber(p, "scale", "part", 0, 0, 24, true);
      if (const auto *x = p.find("parameters"))
        parameters(*x, "part.parameters");
    }
    if (const auto *x = c.find("parameters"))
      parameters(*x, "clip.parameters");
    if (const auto *x = c.find("loop")) {
      keys(*x, "clip.loop",
           {"durationMs", "beats", "rate", "timing", "takes", "recorded",
            "recordedTempo"});
      optNumber(*x, "durationMs", "clip.loop", 2000, 1, 86400000, true);
      optNumber(*x, "beats", "clip.loop", 4, .0001, 100000);
      optNumber(*x, "recordedTempo", "clip.loop", 120, 1, 999);
      optNumber(*x, "rate", "clip.loop", 1, .25, 4);
      optNumber(*x, "takes", "clip.loop", 1, 0, 255, true);
      optBool(*x, "recorded", "clip.loop", true);
      if (const auto *t = x->find("timing")) {
        const auto &time = string(*t, "clip.loop.timing");
        need(time == "free" || time == "beats", "clip.loop.timing",
             "unsupported timing");
      }
    }
    if (const auto *x = c.find("sequence")) {
      keys(*x, "clip.sequence", {"tracks", "mix"});
      for (const auto &lane :
           array(field(*x, "tracks", "sequence"), "sequence.tracks", 4, 4))
        track(lane, "sequence.track");
      if (const auto *m = x->find("mix")) {
        keys(*m, "sequence.mix",
             {"strips", "priority", "depthDb", "thresholdDb", "attackMs",
              "holdMs", "releaseMs", "hpfHz", "kneeDb"});
        if (const auto *strips = m->find("strips"))
          for (const auto &strip : array(*strips, "mix.strips", 4, 4)) {
            keys(strip, "strip",
                 {"pan", "lowDb", "midDb", "highDb", "midHz", "solo", "muted"});
            optNumber(strip, "pan", "strip", 0, -1, 1);
            for (auto k : {"lowDb", "midDb", "highDb"})
              optNumber(strip, k, "strip", 0, -12, 12);
            optNumber(strip, "midHz", "strip", 900, 120, 8000);
            optBool(strip, "solo", "strip", false);
            optBool(strip, "muted", "strip", false);
          }
        optNumber(*m, "priority", "mix", 0, 0, 4, true);
        optNumber(*m, "depthDb", "mix", 0, 0, 24);
        optNumber(*m, "thresholdDb", "mix", -24, -60, 0);
        optNumber(*m, "attackMs", "mix", 12, 1, 250);
        optNumber(*m, "holdMs", "mix", 40, 0, 1000);
        optNumber(*m, "releaseMs", "mix", 220, 10, 3000);
        optNumber(*m, "hpfHz", "mix", 100, 20, 2000);
        optNumber(*m, "kneeDb", "mix", 6, 0, 24);
      }
    } else
      need(mode != 6, "clip.sequence", "required for Sequence mode");
    if (const auto *x = c.find("notes")) {
      const auto &notes = array(*x, "clip.notes", 1024);
      noteCount += notes.size();
      for (const auto &n : notes) {
        keys(n, "note",
             {"timeMs", "durationMs", "note", "velocity", "pad", "part", "take",
              "engineEntry", "engine", "pressure", "variation"});
        number(field(n, "timeMs", "note"), "note.timeMs", 0, 86400000, true);
        number(field(n, "durationMs", "note"), "note.durationMs", 1, 86400000,
               true);
        number(field(n, "note", "note"), "note.note", 0, 127, true);
        number(field(n, "velocity", "note"), "note.velocity", 0, 127, true);
        optNumber(n, "pad", "note", 255, 0, 255, true);
        optNumber(n, "part", "note", 0, 0, 3, true);
        optNumber(n, "take", "note", 0, 0, 255, true);
        {
          const auto e = optNumber(n, "engineEntry", "note", 255, 0, 255, true);
          need(e < 8 || e == 255, "note.engineEntry",
               "expected list entry 0-7 or 255");
        }
        if (const auto *e = n.find("engine"); e && !e->isNull())
          choice(*e, "note.engine", engines);
        optNumber(n, "pressure", "note", 0, 0, 1);
        if (const auto *v = n.find("variation")) {
          keys(*v, "note.variation", {"octave", "position", "stack", "pan"});
          optNumber(*v, "octave", "variation", 0, -4, 4, true);
          for (auto k : {"position", "stack", "pan"})
            optNumber(*v, k, "variation", 0, -1, 1);
        }
      }
    }
    if (const auto *x = c.find("automation")) {
      std::size_t perClip = 0, windowCount = 0;
      for (const auto &curve : array(*x, "clip.automation", 8192)) {
        keys(curve, "curve", {"target", "points", "windows"});
        if(const auto* windows=curve.find("windows"))for(const auto& window:array(*windows,"curve.windows",128)){need(++windowCount<=128,"clip.automation","too many touch windows");const auto& pair=array(window,"curve.window",2,2);number(pair[0],"window.startMs",0,86400000,true);number(pair[1],"window.durationMs",1,86400000,true);}
        const auto &target = field(curve, "target", "curve");
        keys(target, "curve.target", {"type", "key", "index", "owner", "take"});
        choice(field(target, "type", "curve.target"), "curve.type", pointTypes);
        if (const auto *k = target.find("key"))
          need(parameter(string(*k, "curve.key")), "curve.key",
               "unknown parameter");
        optNumber(target, "index", "curve.target", 0, 0, 255, true);
        optNumber(target, "owner", "curve.target", 255, 0, 255, true);
        optNumber(target, "take", "curve.target", 0, 0, 255, true);
        const auto &points =
            array(field(curve, "points", "curve"), "curve.points", 8192, 1);
        pointCount += points.size();
        perClip += points.size();
        need(perClip <= 8192, "clip.automation", "too many control points");
        double previous = -1;
        for (const auto &p : points) {
          const auto &pair = array(p, "curve.point", 2, 2);
          const auto at = number(pair[0], "curve.timeMs", 0, 86400000, true);
          need(at >= previous, "curve.timeMs", "points must be sorted");
          previous = at;
          number(pair[1], "curve.value", -1000000, 1000000);
        }
      }
    }
  }
  need(noteCount <= 16384 && pointCount <= 65536, "composition",
       "event budget exceeded");
  if (const auto *chains = doc.find("chains")) {
    for (const auto &chain : array(*chains, "chains", 16)) {
      keys(chain, "chain", {"name", "end", "fade", "entries"});
      need(label(string(field(chain, "name", "chain"), "chain.name", 40)),
           "chain.name", "invalid saved name");
      const auto &end = string(field(chain, "end", "chain"), "chain.end");
      need(end == "loop" || end == "stay" || end == "stop", "chain.end",
           "unsupported ending");
      const auto fade = [&](const J &f) {
        keys(f, "fade", {"mode", "timeMs", "lowMs", "highMs", "curve"});
        const auto &mode = string(field(f, "mode", "fade"), "fade.mode");
        need(mode == "default" || mode == "cut" || mode == "fixed" ||
                 mode == "random",
             "fade.mode", "unsupported fade mode");
        if (f.find("lowMs") && f.find("highMs"))
          need(f.at("lowMs").number() <= f.at("highMs").number(), "fade",
               "lowMs must not exceed highMs");
        for (auto k : {"timeMs", "lowMs", "highMs"})
          optNumber(f, k, "fade", 0, 0, 30000);
        if (const auto *curve = f.find("curve")) {
          const auto &n = string(*curve, "fade.curve");
          need(n == "linear" || n == "in" || n == "out" || n == "smooth" ||
                   n == "power",
               "fade.curve", "unsupported curve");
        }
      };
      if (const auto *f = chain.find("fade"))
        fade(*f);
      for (const auto &e :
           array(field(chain, "entries", "chain"), "chain.entries", 32)) {
        keys(e, "entry", {"clip", "repeats", "fade", "enabled"});
        need(clips.count(string(field(e, "clip", "entry"), "entry.clip", 64)),
             "entry.clip", "unknown clip");
        optNumber(e, "repeats", "entry", 1, 0, 16, true);
        optBool(e, "enabled", "entry", true);
        if (const auto *f = e.find("fade"))
          fade(*f);
      }
    }
  }
  if (const auto *ext = doc.find("extensions")) {
    object(*ext, "extensions");
    if (const auto *native = ext->find("s3gThru")) {
      keys(*native, "extensions.s3gThru", {"version", "clips"});
      number(field(*native, "version", "extensions.s3gThru"),
             "extensions.s3gThru.version", 1, 1, true);
      const auto &records = field(*native, "clips", "native");
      object(records, "native.clips");
      need(records.objectValue().size() <= 64, "native.clips",
           "too many capsules");
      for (const auto &[id, record] : records.objectValue()) {
        need(safeId(id), "native.clips", "invalid capsule ID");
        keys(record, "native.clip", {"file", "assets", "bankNames"});
        need(string(field(record, "file", "native.clip"), "native.file") ==
                 "extensions/s3g-thru/" + id + ".bin",
             "native.file", "invalid capsule path");
        for (const auto &a : array(field(record, "assets", "native.clip"),
                                   "native.assets", 512))
          need(assets.count(string(a, "native.asset")), "native.asset",
               "unknown asset");
        if (const auto *names = record.find("bankNames"))
          for (const auto &name : array(*names, "native.bankNames", 16, 16))
            need(label(string(name, "native.bankName", 40)), "native.bankName",
                 "invalid label");
      }
    }
  }
  for (const auto &clip : doc.at("clips").arrayValue())
    if (const auto *ext = clip.find("extensions")) {
      object(*ext, "clip.extensions");
      if (const auto *native = ext->find("s3gThru")) {
        keys(*native, "clip.extensions.s3gThru", {"base"});
        const auto &id = string(field(*native, "base", "clip.native"),
                                "clip.native.base", 64);
        const auto *root = doc.find("extensions");
        const auto *bank = root ? root->find("s3gThru") : nullptr;
        need(bank && bank->at("clips").find(id), "clip.native.base",
             "missing native capsule");
      }
    }
}
} // namespace s3g_thru_pack
