// Temporary performance clipboard. Samples remain referenced by path;
// source data, takes, per-part programs and curve indexes are copied together.
ClipSlot clipClipboard{};
int clipClipboardSource = -1;
std::uint64_t clipClipboardGeneration = 0, chainClipUndoGeneration = 0;
ClipChainEntry clipClipboardEntry{};
bool clipClipboardHasEntry = false, undoHasChainClip = false;
int chainClipUndoIndex = -1;

bool clipClipboardEditAllowed() {
    if (clipRecording || audioRecording()) {
        setError("Stop recording first");
        return false;
    }
    const auto sampleState =
        static_cast<SampleLoadState>(sampleLoadState.load(std::memory_order_acquire));
    if (clipPreparationInFlight() ||
        (sampleState >= SampleLoadState::requested && sampleState <= SampleLoadState::copying) ||
        pendingClipSourceMask || pendingClearMask) {
        setError("Wait: clip or fade");
        return false;
    }
    if (pendingCaptureSaveMask != 0u && !serviceMaintenance()) {
        setError("Wait: sample archive");
        return false;
    }
    return true;
}
bool copyClipToClipboard(int source) {
    if (!clipClipboardEditAllowed())
        return false;
    if (source < 0 || source >= static_cast<int>(clipSlots.size())) {
        setError("Invalid clip slot");
        return false;
    }
    if (source == activeClip) {
        captureActiveClip(true);
        markPersistentStateDirty();
    }
    if (!clipSlots[static_cast<std::size_t>(source)].occupied) {
        setError("Clip slot empty");
        return false;
    }
    clipClipboard = clipSlots[static_cast<std::size_t>(source)];
    clipClipboardSource = source;
    ++clipClipboardGeneration;
    clipClipboardHasEntry = false;
    return true;
}
void pasteClipFromClipboard(int destination) {
    if (!clipClipboardEditAllowed())
        return;
    if (clipClipboardSource < 0) {
        setError("Copy a clip first");
        return;
    }
    if (destination < 0 || destination >= static_cast<int>(clipSlots.size())) {
        setError("Invalid clip slot");
        return;
    }
    if (destination == activeClip && (clipPlaying || clipLaunchPending)) {
        setError("Stop loop to paste");
        return;
    }
    // A prefetched bank is a snapshot of this slot. Never launch stale audio
    // after replacing it; the loader will rebuild it off the audio thread.
    if (clipPrepareState.load(std::memory_order_acquire) ==
            static_cast<int>(ClipPrepareState::ready) &&
        preparedClip.load(std::memory_order_relaxed) == destination) {
        preparedClip.store(-1, std::memory_order_relaxed);
        clipPrepareState.store(static_cast<int>(ClipPrepareState::idle), std::memory_order_release);
    }
    pushClipUndo(destination);
    clipSlots[static_cast<std::size_t>(destination)] = clipClipboard;
    clipSlots[static_cast<std::size_t>(destination)].occupied = true;
    if (destination == activeClip) {
        beginSourceTransitionTail();
        allNotesOff();
        activateStoredClip(destination);
    }
    invalidateChainPrefetch();
    markPersistentStateDirty();
}

std::uint64_t referencedClipMask() const {
    std::uint64_t mask = 0;
    for (const auto &bank : chainBanks)
        for (const auto &entry : bank.entries)
            if (entry.enabled && entry.clip < kClipSlotCount)
                mask |= std::uint64_t{1} << entry.clip;
    for (int index : {activeClip, queuedClip, preparedClip.load(std::memory_order_relaxed)})
        if (index >= 0 && index < static_cast<int>(kClipSlotCount))
            mask |= std::uint64_t{1} << index;
    return mask;
}
void copyChainClip() {
    const auto entry = clipChain()[static_cast<std::size_t>(chainEditSlot)];
    if (!entry.enabled) {
        setError("Choose a clip entry");
        return;
    }
    if (copyClipToClipboard(entry.clip)) {
        clipClipboardEntry = entry;
        clipClipboardHasEntry = true;
    }
}
void pasteChainClip() {
    if (!clipClipboardEditAllowed())
        return;
    if (clipClipboardSource < 0) {
        setError("Copy a clip first");
        return;
    }
    const std::uint64_t used = occupiedClipMask() | referencedClipMask();
    int target = -1;
    const int preferred = (clipClipboardSource / 16) * 16;
    for (int offset = 0; offset < static_cast<int>(kClipSlotCount); ++offset) {
        const int index = (preferred + offset) % kClipSlotCount;
        if (!(used & (std::uint64_t{1} << index))) {
            target = index;
            break;
        }
    }
    if (target < 0) {
        setError("No unused clip slots");
        return;
    }
    pushChainUndo("duplicate clip", chainEditSlot, true);
    undoClip = clipSlots[static_cast<std::size_t>(target)];
    undoClipIndex = target;
    clipSlots[static_cast<std::size_t>(target)] = clipClipboard;
    auto &entry = clipChain()[static_cast<std::size_t>(chainEditSlot)];
    if (clipClipboardHasEntry)
        entry = clipClipboardEntry;
    else if (!entry.enabled)
        entry = ClipChainEntry{};
    entry.enabled = true;
    entry.clip = static_cast<std::uint8_t>(target);
    undoHasChainClip = true;
    chainClipUndoIndex = target;
    chainClipUndoGeneration = clipClipboardGeneration;
    invalidateChainPrefetch();
    markPersistentStateDirty();
}
void restoreChainClipUndo() {
    if (!undoHasChainClip)
        return;
    const int index = chainClipUndoIndex;
    // Keep a duplicate which has since been launched, referenced elsewhere or
    // changed. An immediate undo also returns its unused slot to the clip bank.
    if (index >= 0 && index < static_cast<int>(kClipSlotCount) &&
        chainClipUndoGeneration == clipClipboardGeneration &&
        std::memcmp(&clipSlots[static_cast<std::size_t>(index)], &clipClipboard,
                    sizeof(ClipSlot)) == 0) {
        if (queuedClip != index && activeClip != index &&
            clipPrepareState.load(std::memory_order_acquire) ==
                static_cast<int>(ClipPrepareState::ready) &&
            preparedClip.load() == index) {
            preparedClip.store(-1);
            clipPrepareState.store(static_cast<int>(ClipPrepareState::idle),
                                   std::memory_order_release);
        }
        if (!(referencedClipMask() & (std::uint64_t{1} << index)))
            clipSlots[static_cast<std::size_t>(index)] = undoClip;
    }
    undoHasChainClip = false;
    chainClipUndoIndex = -1;
}
