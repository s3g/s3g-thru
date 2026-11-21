static bool writeLfoAnchor(std::FILE *f, double v) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &v, 8);
  return writeU64(f, bits);
}
static bool readLfoAnchor(std::FILE *f, double &v) {
  std::uint64_t bits = 0;
  if (!readU64(f, bits))
    return false;
  std::memcpy(&v, &bits, 8);
  return std::isfinite(v);
}
bool writeControlLfos(std::FILE *file,
                      const std::array<ClipSlot, kClipSlotCount> &clips) const {
  bool ok = writeU32(file, 0x314f464cu);
  for (const auto &c : clips) {
    for (int group = 0; group < 5; ++group) {
      const auto &part = group ? c.splitSettings[group - 1] : c.singleSettings;
      for (const auto &l : part.lfos) {
        ok = ok && s3g_thru_lfo::valid(l);
        for (float v : l.v)
          ok = ok && writeF32(file, v);
        ok = ok && writeLfoAnchor(file, l.anchor) && writeU32(file, l.seed) &&
             writeU32(file, l.cycle) && writeU32(file, l.captured ? 1 : 0) &&
             writeF32(file, l.outputAnchor);
        for (float x : l.routeAnchor)
          ok = ok && writeF32(file, x);
        ok = ok && writeU32(file, l.runningAnchor ? 1u : 0u);
      }
    }
    for (const auto &lane : c.sequence.tracks)
      for (int set = 0; set < 2; ++set)
        for (const auto &step : set ? lane.generator.base : lane.trigs) {
          const auto &l = step.lfos;
          ok = ok && s3g_thru_lfo::valid(l) && writeU32(file, l.mask & 15u) &&
               writeU32(file, l.reset & 3u);
          for (int i = 0; i < 2; ++i)
            ok =
                ok && writeF32(file, l.rates[i]) && writeF32(file, l.depths[i]);
        }
  }
  return ok;
}
static bool readControlLfos(std::FILE *file,
                            std::array<ClipSlot, kClipSlotCount> &clips) {
  std::uint32_t magic = 0;
  if (!readU32(file, magic) || magic != 0x314f464cu)
    return false;
  for (auto &c : clips) {
    for (int group = 0; group < 5; ++group) {
      auto &part = group ? c.splitSettings[group - 1] : c.singleSettings;
      for (auto &l : part.lfos) {
        for (float &v : l.v)
          if (!readF32(file, v))
            return false;
        double anchor = 0;
        std::uint32_t captured = 0;
        if (!readLfoAnchor(file, anchor) || !readU32(file, l.seed) ||
            !readU32(file, l.cycle) || !readU32(file, captured) || captured > 1)
          return false;
        if (!readF32(file, l.outputAnchor))
          return false;
        for (float &x : l.routeAnchor)
          if (!readF32(file, x))
            return false;
        std::uint32_t running = 0;
        if (!readU32(file, running) || running > 1)
          return false;
        l.runningAnchor = running != 0;
        l.anchor = anchor;
        l.captured = captured;
        if (!s3g_thru_lfo::valid(l))
          return false;
      }
    }
    for (auto &lane : c.sequence.tracks)
      for (int set = 0; set < 2; ++set)
        for (auto &step : set ? lane.generator.base : lane.trigs) {
          auto &l = step.lfos;
          std::uint32_t mask = 0, reset = 0;
          if (!readU32(file, mask) || mask > 15 || !readU32(file, reset) ||
              reset > 3)
            return false;
          l.mask = std::uint8_t(mask);
          l.reset = std::uint8_t(reset);
          for (int i = 0; i < 2; ++i)
            if (!readF32(file, l.rates[i]) || !readF32(file, l.depths[i]))
              return false;
          if (!s3g_thru_lfo::valid(l))
            return false;
        }
  }
  return true;
}

bool readLegacyControlLfoJson(const char *json) {
  for (int g = 0; g < 5; ++g)
    for (int slot = 0; slot < 2; ++slot) {
      char key[24]{};
      std::snprintf(key, sizeof(key), "\"lfo_%d_%d\"", g, slot);
      const char *at = std::strstr(json, key);
      if (!at)
        continue;
      at = std::strchr(at, ':');
      if (!at)
        return false;
      ++at;
      while (*at == ' ' || *at == '\n' || *at == '\r' || *at == '\t')
        ++at;
      if (*at++ != '[')
        return false;
      double numbers[30]{};
      for (int i = 0; i < 30; ++i) {
        char *end = nullptr;
        numbers[i] = std::strtod(at, &end);
        if (end == at || !std::isfinite(numbers[i]))
          return false;
        at = end;
        while (*at == ' ' || *at == '\n' || *at == '\r' || *at == '\t')
          ++at;
        if (*at++ != (i == 29 ? ']' : ','))
          return false;
      }
      s3g_thru_lfo::Config c;
      for (int f = 0; f < 9; ++f)
        c.v[f] = float(numbers[f]);
      c.anchor = numbers[9];
      if (numbers[10] < 0 || numbers[10] > 4294967295.0 ||
          numbers[10] != std::floor(numbers[10]) || numbers[11] < 0 ||
          numbers[11] > 4294967295.0 ||
          numbers[11] != std::floor(numbers[11]) ||
          (numbers[12] != 0 && numbers[12] != 1))
        return false;
      c.seed = std::uint32_t(numbers[10]);
      c.cycle = std::uint32_t(numbers[11]);
      c.captured = numbers[12] != 0;
      c.outputAnchor = float(numbers[13]);
      for (int d = 0; d < 15; ++d)
        c.routeAnchor[d] = float(numbers[14 + d]);
      if (numbers[29] != 0 && numbers[29] != 1)
        return false;
      c.runningAnchor = numbers[29] != 0;
      if (!s3g_thru_lfo::valid(c))
        return false;
      if (g == 0) {
        splitSingleSettings.lfos[slot] = c;
        if (!splitEnabled)
          legacyControlLfos[slot] = c;
      } else {
        splitParts[g - 1].settings.lfos[slot] = c;
        if (splitEnabled && g - 1 == splitFocus)
          legacyControlLfos[slot] = c;
      }
    }
  return true;
}
