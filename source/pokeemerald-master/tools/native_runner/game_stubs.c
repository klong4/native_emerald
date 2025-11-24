// Comprehensive stubs for game symbols to enable linking the native runner
// These are placeholder implementations that allow compilation without the full game codebase

#include "gba_shim.h"

// Text strings
const u8 gText_WhatWillPkmnDo[] = "What will PKMN do?";
const u8 gText_LinkStandby[] = "Link standby...";
const u8 gText_BattleMenu[] = "FIGHT/BAG/PKMN/RUN";
const u8 gText_MoveInterfacePP[] = "PP";
const u8 gText_MoveInterfaceType[] = "TYPE/";
const u8 gText_BattleYesNoChoice[] = "YES\nNO";
const u8 gText_BattleSwitchWhich[] = "Switch which?";

// Main global structure
struct Main {
    u16 state;
    u16 savedCallback;
    u32 vblankCounter1;
    u32 vblankCounter2;
    u16 heldKeysRaw;
    u16 newKeysRaw;
    u16 heldKeys;
    u16 newKeys;
    u16 newAndRepeatedKeys;
    u16 keyRepeatCounter;
    u8 field_padding[0x100];
};
struct Main gMain = {0};

// Battle globals
s16 gBattle_BG0_X = 0;
s16 gBattle_BG0_Y = 0;
u8 gDisplayedStringBattle[256] = {0};
u8 gBattlerSpriteIds[4] = {0};

// Disable structures
struct DisableStruct {
    u32 transformedMonPersonality;
    u16 disabledMove;
    u16 encoredMove;
    u8 disableTimer;
    u8 encoreTimer;
    u8 perishSongTimer;
    u8 furyCutterCounter;
    u8 rolloutTimer;
    u8 chargeTimer;
    u8 tauntTimer;
    u8 battlerPreventingEscape;
    u8 battlerWithSureHit;
    u8 isFirstTurn;
};
struct DisableStruct gDisableStructs[4] = {0};

// Battle sprite/animation globals
u8 gIntroSlideFlags = 0;
u8 gBattlerStatusSummaryTaskId[4] = {0};
u8 gBattlerInMenuId = 0;
u8 gDoingBattleAnim = 0;
u32 gTransformedPersonalities[4] = {0};
u16 gPlayerDpadHoldFrames = 0;

// Battle sprite data pointer
void *gBattleSpritesDataPtr = NULL;

// Battle forms
u8 gBattleMonForms[4] = {0};

// Pre-battle callback
void (*gPreBattleCallback1)(void) = NULL;

// Healthbox sprite IDs
u8 gHealthboxSpriteIds[4] = {0};

// Multi-use cursor
u8 gMultiUsePlayerCursor = 0;

// Move choice
u8 gNumberOfMovesToChoose = 0;

// Battle controller data
u8 *gBattleControllerData[4] = {NULL, NULL, NULL, NULL};

// Animation script callback/active
void (*gAnimScriptCallback)(void) = NULL;
u8 gAnimScriptActive = 0;

// Animation disable struct pointer
struct DisableStruct *gAnimDisableStructPtr = NULL;

// Animation damage/power/friendship
s32 gAnimMoveDmg = 0;
u16 gAnimMovePower = 0;
u8 gAnimFriendship = 0;

// Weather move animation
u16 gWeatherMoveAnim = 0;

// Animation move turn
u8 gAnimMoveTurn = 0;

// Player party lost HP
u16 gPlayerPartyLostHP = 0;

// Partner trainer ID
u16 gPartnerTrainerId = 0;

// Trainer graphics tables (placeholder pointers)
const struct {
    u8 size;
    u8 y_offset;
} gTrainerFrontPicCoords[256] = {0};

const u32 *gTrainerFrontPicPaletteTable[256] = {NULL};

const struct {
    u8 size;
    u8 y_offset;
} gTrainerBackPicCoords[256] = {0};

const u32 *gTrainerBackPicPaletteTable[256] = {NULL};

// Move names
const u8 gMoveNames[512][13] = {0};

// Special var item ID
u16 gSpecialVar_ItemId = 0;

// Music playback info
struct MusicPlayerInfo {
    void *songHeader;
    u32 status;
    u8 trackCount;
    u8 priority;
    u8 cmd;
    u8 unk_B;
};
struct MusicPlayerInfo gMPlayInfo_BGM = {0};

// Palette fade
struct PaletteFadeControl {
    u32 multipurpose1;
    u8 delayCounter;
    u8 y;
    u16 targetY;
    u16 blendCnt;
    u16 blendAlpha;
    u16 blendY;
    u8 active;
    u8 multipurpose2;
    u8 yDec;
    u8 bufferTransferDisabled;
    u8 shouldResetBlendRegisters;
    u8 hardwareFadeFinishing;
    u8 softwareFadeFinishingCounter;
    u8 softwareFadeFinishing;
    u8 objPaletteToggle;
    u32 deltaY;
};
struct PaletteFadeControl gPaletteFade = {0};

// Party menu callback
u8 gPartyMenuUseExitCallback = 0;

// Selected mon party ID
u8 gSelectedMonPartyId = 0;

// Battle party current order
u8 gBattlePartyCurrentOrder[6] = {0};

// RNG value
u32 gRngValue = 12345;

// Battle palace move selection RNG
u32 gBattlePalaceMoveSelectionRngValue = 0;

// Function stubs
void GetMonData(void *mon, int request, u8 *dst) {
    // Stub: returns zeros
    if (dst) *dst = 0;
}

u32 GetMonData1(void *mon, int request) {
    // Stub: returns 0
    return 0;
}

void SetMonData(void *mon, int request, const void *value) {
    // Stub: no-op
}

void BattleLoadOpponentMonSpriteGfx(void *mon, int battler) {
    // Stub: no-op
}

void HandleBattleWindow(u8 xStart, u8 yStart, u8 xEnd, u8 yEnd, u8 flags) {
    // Stub: no-op
}

void CopyBattleWindowToVram(u8 *src, u16 windowId, u8 mode) {
    // Stub: no-op
}

void sub_8045A5C(u8 windowId, u8 copyToVram, const u8 *text, u8 speed) {
    // Stub: no-op
}

void BattlePutTextOnWindow(const u8 *text, u8 windowId) {
    // Stub: no-op
}

void BufferStringBattle(u16 stringID) {
    // Stub: no-op
}

u8 CreateTask(void (*func)(u8), u8 priority) {
    // Stub: returns dummy task ID
    return 0;
}

void DestroyTask(u8 taskId) {
    // Stub: no-op
}

u8 CreateSprite(const void *template, s16 x, s16 y, u8 subpriority) {
    // Stub: returns dummy sprite ID
    return 0;
}

void DestroySprite(void *sprite) {
    // Stub: no-op
}

void *gSprites = NULL;

void LoadCompressedSpriteSheet(const void *sheet) {
    // Stub: no-op
}

void LoadCompressedSpritePalette(const void *pal) {
    // Stub: no-op
}

void SetSubspriteTables(void *sprite, const void *table) {
    // Stub: no-op
}

void AnimateSprite(void *sprite) {
    // Stub: no-op
}

void FreeSpriteOamMatrix(void *sprite) {
    // Stub: no-op
}

void FreeSpriteTilesByTag(u16 tag) {
    // Stub: no-op
}

void FreeSpritePaletteByTag(u16 tag) {
    // Stub: no-op
}

void BeginNormalPaletteFade(u32 selectedPalettes, s8 delay, u8 startY, u8 targetY, u16 blendColor) {
    // Stub: no-op
}

u8 UpdatePaletteFade(void) {
    // Stub: returns "not fading"
    return 0;
}

u8 GetBattlerPosition(u8 battler) {
    // Stub: returns position 0
    return 0;
}

u8 GetBattlerAtPosition(u8 position) {
    // Stub: returns battler 0
    return 0;
}

u8 GetBattlerSide(u8 battler) {
    // Stub: returns side 0 (player)
    return battler & 1;
}

void EmitGetMonData(u8 bufferId, u8 requestId, u8 *dst) {
    // Stub: no-op
}

void MarkBattlerForControllerExec(u8 battlerId) {
    // Stub: no-op
}

void sub_8052E44(u8 windowId, u16 itemId, u8 y, u8 *usedItemCount) {
    // Stub: no-op
}

void sub_81DB3CC(void) {
    // Stub: no-op
}

void ChoosePokemon(void (*callback)(void), u8 battler) {
    // Stub: no-op
}

void DoSwitchOutAnimation(void) {
    // Stub: no-op
}

void BattleCreateYesNoDynMultichoiceMenu(u8 windowId, u16 multichoiceId, u8 initialCursorPos, u8 unused) {
    // Stub: no-op
}

void MoveSelectionDisplayMoveNames(void) {
    // Stub: no-op
}

void MoveSelectionDisplayPpNumber(void) {
    // Stub: no-op
}

void MoveSelectionDisplayPpString(void) {
    // Stub: no-op
}

void MoveSelectionDisplayMoveType(void) {
    // Stub: no-op
}

void MoveSelectionCreateCursorAt(u8 cursorPosition, u8 baseTileNum) {
    // Stub: no-op
}

void MoveSelectionDestroyCursorAt(u8 cursorPosition) {
    // Stub: no-op
}

void BtlController_HandleInputChooseTarget(u8 battler, u16 *move, void (*callback)(void)) {
    // Stub: no-op
}


void PlaySE(u16 songNum) {
    // Stub: no-op
}

void PlayCry1(u16 species, s8 pan) {
    // Stub: no-op
}

void sub_806E694(u8 battler) {
    // Stub: no-op
}

void sub_806E76C(u8 battler) {
    // Stub: no-op
}

void sub_8067C54(u8 battler) {
    // Stub: no-op
}

void BattleLoadMonSpriteGfx(void *mon, u8 battlerId) {
    // Stub: no-op
}

void StartSpriteAnim(void *sprite, u8 animNum) {
    // Stub: no-op
}

void sub_8075DB4(u8 battler, u8 b) {
    // Stub: no-op
}

void UpdateHealthboxAttribute(u8 healthboxSpriteId, void *mon, u8 elementId) {
    // Stub: no-op
}

void StartHealthboxSlideIn(u8 battler) {
    // Stub: no-op
}

void PlayBattleBGM(void) {
    // Stub: no-op
}


void m4aMPlayContinue(struct MusicPlayerInfo *mplayInfo) {
    // Stub: no-op
}

void sub_805D770(u8 battlerId) {
    // Stub: no-op
}

void HandleLowHpMusicChange(void *mon, u8 battler) {
    // Stub: no-op
}

void BattleInterfaceSetWindowPals(void) {
    // Stub: no-op
}

void LoadBattleMenuWindowGfx(void) {
    // Stub: no-op
}

void sub_8045A5C_2(u8 windowId, u8 copyToVram, const u8 *text, u8 speed) {
    // Stub: no-op
}

void BattleStringExpandPlaceholdersToDisplayedString(const u8 *src) {
    // Stub: no-op
}

void ExpandBattleTextBuffPlaceholders(const u8 *src, u8 *dst) {
    // Stub: no-op
}

u8 BattleSetup_GetTerrainId(void) {
    // Stub: returns terrain 0
    return 0;
}

u16 ChooseMoveAndTargetInBattlePalace(void) {
    // Stub: returns move 0 with target 0
    return 0;
}

void RecordedOpponentBufferExecCompleted(void) {
    // Stub: no-op
}

void sub_805D714(u8 battlerId, u8 b) {
    // Stub: no-op
}

void BattleLoadSubstituteOrMonSpriteGfx(u8 battlerId, u8 b) {
    // Stub: no-op
}

void BattleGfxSfxDummy3(u8 battler) {
    // Stub: no-op
}

void sub_805D8AC(u8 battlerId, u8 b) {
    // Stub: no-op
}

void ClearTemporarySpeciesSpriteData(u8 battlerId, u8 b) {
    // Stub: no-op
}

void AllocateBattleSpritesData(void) {
    // Stub: no-op
}

void sub_805D890(u8 battlerId) {
    // Stub: no-op
}

u32 GetSpritePalTagFromSpeciesAndPersonality(u16 species, u32 otId, u32 personality) {
    // Stub: returns 0
    return 0;
}

void sub_8074634(u8 battlerId) {
    // Stub: no-op
}

void DoHitAnimHealthboxEffect(s32 battler) {
    // Stub: no-op
}

void UpdateHpTextInHealthbox(u8 healthboxSpriteId, s16 value, u8 maxOrCurrent) {
    // Stub: no-op
}

void SetBattleBarStruct(u8 battler, u8 healthboxSpriteId, s32 maxVal, s32 currVal, s32 receivedValue) {
    // Stub: no-op
}

void MoveBattleBar(u8 battler, u8 healthboxSpriteId, u8 whichBar, u8 arg3) {
    // Stub: no-op
}

void sub_8043DFC(u8 battler) {
    // Stub: no-op
}

void sub_8079F98(void) {
    // Stub: no-op
}

void sub_8079F60(void) {
    // Stub: no-op
}

u8 IsDoubleBattle(void) {
    // Stub: returns false
    return 0;
}

u8 IsLinkDoubleBattle(void) {
    // Stub: returns false
    return 0;
}

void AnimTask_ShakeMon(u8 taskId) {
    // Stub: no-op
}

void sub_8078250(u8 taskId) {
    // Stub: no-op
}

u8 LaunchBattleAnimation(u8 animId, u16 move) {
    // Stub: returns dummy anim ID
    return 0;
}

void ResetSpriteRotScale(void *sprite) {
    // Stub: no-op
}

void PlayFanfareOrBGM(u16 songId) {
    // Stub: no-op
}

void sub_8078C04(u8 battlerId, const void *ptr) {
    // Stub: no-op
}

void sub_8078C5C(u8 battlerId, u8 b) {
    // Stub: no-op
}


void sub_806F2AC(u8 battler, void *a) {
    // Stub: no-op
}

void sub_806F324(u8 battler, void *a, u8 b) {
    // Stub: no-op
}

void TryShinyAnimation(u8 battler, void *mon) {
    // Stub: no-op
}

void BattleLoadMonSpriteGfxUnused(void *mon, u8 battlerId, u8 b, u8 flags) {
    // Stub: no-op
}

void GetBestDmgFromBattleScript(u8 battler, u8 *buffer) {
    // Stub: no-op
}

void InitAndLaunchChooseMoveYesNoBox(u8 battler) {
    // Stub: no-op
}

u8 ChooseMoveAndTargetInBattlePyramid(void) {
    // Stub: returns 0
    return 0;
}

void CompressAndLoadBgGfxUsingHeap(u8 bg, const void *src, u16 size, u16 offset, u8 mode) {
    // Stub: no-op
}

void LoadCompressedPalette(const void *src, u16 offset, u16 size) {
    // Stub: no-op
}

void LoadPalette(const void *src, u16 offset, u16 size) {
    // Stub: no-op
}

void CopyToWindowPixelBuffer(u8 windowId, const void *src, u16 size, u16 mode) {
    // Stub: no-op
}

void CopyWindowToVram(u8 windowId, u8 mode) {
    // Stub: no-op
}

void SetBgAttribute(u8 bg, u8 attributeId, u16 value) {
    // Stub: no-op
}

void ShowBg(u8 bg) {
    // Stub: no-op
}

void HideBg(u8 bg) {
    // Stub: no-op
}

void sub_8080CCC(void) {
    // Stub: no-op
}

void sub_8075BA4(u8 a, u8 b, u8 c, u8 d, u8 e) {
    // Stub: no-op
}

void SetHealthboxSpriteVisible(u8 healthboxSpriteId) {
    // Stub: no-op
}

void SetHealthboxSpriteInvisible(u8 healthboxSpriteId) {
    // Stub: no-op
}

void DestroySpriteAndFreeResources(void *sprite) {
    // Stub: no-op
}

void FreeAllSpritePalettes(void) {
    // Stub: no-op
}

void ResetPaletteFade(void) {
    // Stub: no-op
}

void UnlockPlayerFieldControls(void) {
    // Stub: no-op
}

void ScriptContext1_SetupScript(const u8 *ptr) {
    // Stub: no-op
}

void ScriptContext2_Enable(void) {
    // Stub: no-op
}

void sub_8044CA0(u16 a) {
    // Stub: no-op
}

void sub_80451F8(u8 windowId, void *tilemap, u16 baseBlock) {
    // Stub: no-op
}

void sub_80451E4(u8 windowId, u8 copyToVram) {
    // Stub: no-op
}

void sub_804537C(u8 windowId, u16 destOffset) {
    // Stub: no-op
}

void PlaySE12WithPanning(u16 songNum, s8 pan) {
    // Stub: no-op
}

void StopMapMusic(void) {
    // Stub: no-op
}

u8 GetScaledHPFraction(s16 hp, s16 maxhp, u8 scale) {
    // Stub: returns 0
    if (maxhp == 0) return 0;
    return (hp * scale) / maxhp;
}

u8 GetMonGender(void *mon) {
    // Stub: returns male
    return 0;
}

u16 GetMonSpritePalFromSpeciesAndPersonality(u16 species, u32 otId, u32 personality) {
    // Stub: returns 0
    return 0;
}

void HandleBattleLowHpMusicChange(void) {
    // Stub: no-op
}

void sub_806E8F0(u8 battlerId, u16 b, u8 c) {
    // Stub: no-op
}

void BattleAI_SetupAIData(u8 defaultScoreMoves) {
    // Stub: no-op
}

void PlayerPartnerHandleGetMonData(void) {
    // Stub: no-op
}

const void *GetTrainerBackPic(u16 trainerId, u8 gender) {
    // Stub: returns NULL
    return NULL;
}

const void *GetTrainerFrontPic(u16 trainerId, u8 gender) {
    // Stub: returns NULL
    return NULL;
}

void DecompressTrainerBackPic(u16 trainerId, u8 battlerId) {
    // Stub: no-op
}

void DecompressTrainerFrontPic(u16 trainerId, u8 battlerId) {
    // Stub: no-op
}

void FreeTrainerFrontPicPalette(u16 trainerId) {
    // Stub: no-op
}

void sub_8062C68(u8 battler) {
    // Stub: no-op
}

void HandleBattleWindowTilemap(u8 windowId, u8 mode, u16 baseTile, u8 paletteNum, u8 bg) {
    // Stub: no-op
}

void sub_8071DA4(u8 battler) {
    // Stub: no-op
}

void LinkOpponentBufferExecCompleted(void) {
    // Stub: no-op
}

void SetBattleBarStats(u8 battler, u8 healthboxSpriteId, s32 maxVal, s32 currVal, s32 receivedValue) {
    // Stub: no-op
}

void TryShinyAnimAfterMonSlideIn(u8 battler, void *mon, u8 a) {
    // Stub: no-op
}

u32 CalculatePPWithBonus(u16 move, u8 ppBonuses, u8 moveSlot) {
    // Stub: returns 0
    return 0;
}

u8 MovePpIsMaxedOut(void *mon, u8 moveSlot) {
    // Stub: returns 0
    return 0;
}

u16 ItemIdToBattleMoveId(u16 item) {
    // Stub: returns 0
    return 0;
}

void DrawBattlerOnBg(u8 bg, u8 battlerId, u8 x, u8 y, u8 paletteSlot, const void *buffer) {
    // Stub: no-op
}

// Additional missing symbols (second batch)
u8 IsBattleSEPlaying(void) {
    return 0;
}

void BattleLoadPlayerMonSpriteGfx(void *mon, u8 battlerId) {
    // Stub: no-op
}

void BattleGfxSfxDummy2(u8 battler) {
    // Stub: no-op
}

void LoadBattleBarGfx(u8 battlerId) {
    // Stub: no-op
}

void CopyAllBattleSpritesInvisibilities(void) {
    // Stub: no-op
}

void CopyBattleSpriteInvisibility(u8 battlerId) {
    // Stub: no-op
}

void TrySetBehindSubstituteSpriteBit(u8 battlerId, u16 move) {
    // Stub: no-op
}

void BattleStopLowHpSound(void) {
    // Stub: no-op
}

void SetBattlerSpriteAffineMode(u8 affineMode) {
    // Stub: no-op
}

void DoPokeballSendOutAnimation(s16 a, u8 b) {
    // Stub: no-op
}

void SetMainCallback2(void (*func)(void)) {
    // Stub: no-op
}

void TaskDummy(u8 taskId) {
    // Stub: no-op
}

void DoMoveAnim(u16 move) {
    // Stub: no-op
}

void HandleIntroSlide(u8 terrain) {
    // Stub: no-op
}

void StartAnimLinearTranslation(void *sprite) {
    // Stub: no-op
}

u8 GetBattlerSpriteSubpriority(u8 battlerId) {
    return 0;
}

s16 GetBattlerSpriteCoord(u8 battlerId, u8 coordType) {
    return 0;
}

u8 IsBattlerSpritePresent(u8 battlerId) {
    return 0;
}

void StoreSpriteCallbackInData6(void *sprite, void (*callback)(void *)) {
    // Stub: no-op
}

void SetSpritePrimaryCoordsFromSecondaryCoords(void *sprite) {
    // Stub: no-op
}

s16 GetBattlerSpriteDefault_Y(u8 battlerId) {
    return 0;
}

void BattleArena_DeductSkillPoints(u8 battlerId, u8 stringId) {
    // Stub: no-op
}

void SwapHpBarsWithHpText(void) {
    // Stub: no-op
}

void CreatePartyStatusSummarySprites(u8 battlerId, void *partyInfo, u8 a, u8 b) {
    // Stub: no-op
}

void Task_HidePartyStatusSummary(u8 taskId) {
    // Stub: no-op
}

void BattleTv_SetDataBasedOnString(u8 a, const u8 *str) {
    // Stub: no-op
}

void BattleTv_SetDataBasedOnMove(u16 move, u16 b, u8 c) {
    // Stub: no-op
}

void BattleTv_SetDataBasedOnAnimation(u8 a) {
    // Stub: no-op
}

void TryPutLinkBattleTvShowOnAir(void) {
    // Stub: no-op
}

void BattleTv_ClearExplosionFaintCause(void) {
    // Stub: no-op
}

u8 IsDma3ManagerBusyWithBgCopy(void) {
    return 0;
}

void CopyBgTilemapBufferToVram(u8 bg) {
    // Stub: no-op
}

void CopyToBgTilemapBufferRect_ChangePalette(u8 bg, const void *src, u8 destX, u8 destY, u8 width, u8 height, u8 palette) {
    // Stub: no-op
}

u8 AddBagItem(u16 itemId, u16 count) {
    return 1;
}

void FreeAllWindowBuffers(void) {
    // Stub: no-op
}

void CB2_BagMenuFromBattle(void) {
    // Stub: no-op
}

void SetLinkStandbyCallback(void) {
    // Stub: no-op
}

void SetCloseLinkCallback(void) {
    // Stub: no-op
}

void m4aMPlayVolumeControl(struct MusicPlayerInfo *mplayInfo, u8 a, u8 b) {
    // Stub: no-op
}

void m4aSongNumStop(u16 songNum) {
    // Stub: no-op
}

void BeginFastPaletteFade(u8 submode) {
    // Stub: no-op
}

void OpenPartyMenuInBattle(u8 battler) {
    // Stub: no-op
}

void RecordedBattle_RecordAllBattlerData(void *a) {
    // Stub: no-op
}

void ReshowBattleScreenDummy(void) {
    // Stub: no-op
}

void ReshowBattleScreenAfterMenu(void) {
    // Stub: no-op
}

void FadeOutMapMusic(u8 speed) {
    // Stub: no-op
}

void PlayFanfare(u16 fanfareNum) {
    // Stub: no-op
}

void PlayCry_ByMode(u16 species, s8 pan, u8 mode) {
    // Stub: no-op
}

u8 IsCryPlayingOrClearCrySongs(void) {
    return 0;
}

void PlayBGM(u16 songNum) {
    // Stub: no-op
}

u8 *StringCopy_Nickname(u8 *dest, const u8 *src) {
    // Stub: returns dest
    return dest;
}

u8 *StringCopy(u8 *dest, const u8 *src) {
    // Stub: returns dest
    return dest;
}

u8 *ConvertIntToDecimalStringN(u8 *dest, s32 value, u8 mode, u8 n) {
    // Stub: returns dest
    return dest;
}

u8 IsTextPrinterActive(u8 windowId) {
    return 0;
}

u8 CreateInvisibleSpriteWithCallback(void (*callback)(void *)) {
    return 0;
}

// Sprite template
const void *gMultiuseSpriteTemplate = NULL;

// Species info table
const struct {
    u8 baseHP;
    u8 baseAttack;
    u8 baseDefense;
    u8 baseSpeed;
    u8 baseSpAttack;
    u8 baseSpDefense;
    u8 type1;
    u8 type2;
    u8 catchRate;
    u16 expYield;
    u16 evYield_HP:2;
    u16 evYield_Attack:2;
    u16 evYield_Defense:2;
    u16 evYield_Speed:2;
    u16 evYield_SpAttack:2;
    u16 evYield_SpDefense:2;
    u16 item1;
    u16 item2;
    u8 genderRatio;
    u8 eggCycles;
    u8 friendship;
    u8 growthRate;
    u8 eggGroup1;
    u8 eggGroup2;
    u8 abilities[2];
    u8 safariZoneFleeRate;
    u8 bodyColor:7;
    u8 noFlip:1;
} gSpeciesInfo[1024] = {0};

// Experience tables
const u32 gExperienceTables[6][100] = {0};

// Type names
const u8 gTypeNames[18][7] = {0};

// Sprite callbacks and animation stubs (third batch)
void SpriteCB_ShowAsMoveTarget(void *sprite) {
    // Stub: no-op
}

void SpriteCB_HideAsMoveTarget(void *sprite) {
    // Stub: no-op
}

void SpriteCB_FaintSlideAnim(void *sprite) {
    // Stub: no-op
}

void DoBounceEffect(u8 battlerId, u8 b, s8 c, s8 d) {
    // Stub: no-op
}

void EndBounceEffect(u8 battlerId, u8 b) {
    // Stub: no-op
}

void SetPpNumbersPaletteInMoveSelection(void) {
    // Stub: no-op
}

void BattleDestroyYesNoCursorAt(u8 cursorPos) {
    // Stub: no-op
}

void BattleCreateYesNoCursorAt(u8 cursorPos) {
    // Stub: no-op
}

void SpriteCB_WaitForBattlerBallReleaseAnim(void *sprite) {
    // Stub: no-op
}

void SpriteCB_TrainerSlideIn(void *sprite) {
    // Stub: no-op
}

u8 InitAndLaunchChosenStatusAnimation(u8 battlerId, u8 b, u32 c) {
    return 0;
}

u8 TryHandleLaunchBattleTableAnimation(u8 battlerId, u8 b, u8 c, u8 d, u16 e) {
    return 0;
}

u8 InitAndLaunchSpecialAnimation(u8 battlerId, u8 b, u8 c, u16 d) {
    return 0;
}

u8 IsMoveWithoutAnimation(u16 move, u8 animTurn) {
    return 0;
}

// Fourth batch of stubs
u8 IndexOfSpritePaletteTag(u16 tag) {
    return 0;
}

u16 GetSpritePaletteTagByPaletteNum(u8 paletteNum) {
    return 0;
}

void CalculateMonStats(void *mon) {
    // Stub: no-op
}

u8 CountAliveMonsInBattle(u8 battlerId, u8 b) {
    return 1;
}

u8 GetDefaultMoveTarget(u8 battlerId) {
    return 0;
}

void SetMultiuseSpriteTemplateToPokemon(u16 species, u8 battlerPosition) {
    // Stub: no-op
}

void SetMultiuseSpriteTemplateToTrainerBack(u16 trainerId, u8 battlerPosition) {
    // Stub: no-op
}

void SetMultiuseSpriteTemplateToTrainerFront(u16 trainerId, u8 battlerPosition) {
    // Stub: no-op
}

u16 PlayerGenderToFrontTrainerPicId(u8 gender) {
    return 0;
}

void BattleMainCB2(void) {
    // Stub: no-op
}

void CB2_InitEndLinkBattle(void) {
    // Stub: no-op
}

// Fifth batch - link/multiplayer and sprite globals
struct LinkPlayer {
    u8 name[8];
    u8 trainerId[4];
    u16 id;
    u8 language;
    u8 version;
    u8 gender;
    u8 progressFlags;
    u8 neverRead;
    u8 progressFlagsCopy;
    u8 lp_field_a;
    u8 linkType;
    u8 serialNumber;
    u8 playerState;
    u8 sendFlag;
};
struct LinkPlayer gLinkPlayers[4] = {0};

u8 gReceivedRemoteLinkPlayers = 0;

u8 gWirelessCommType = 0;

u8 gRecordedBattleMultiplayerId = 0;

const u32 gBitTable[32] = {
    0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x00000100, 0x00000200, 0x00000400, 0x00000800,
    0x00001000, 0x00002000, 0x00004000, 0x00008000,
    0x00010000, 0x00020000, 0x00040000, 0x00080000,
    0x00100000, 0x00200000, 0x00400000, 0x00800000,
    0x01000000, 0x02000000, 0x04000000, 0x08000000,
    0x10000000, 0x20000000, 0x40000000, 0x80000000
};

void SpriteCallbackDummy(void *sprite) {
    // Stub: no-op
}

void FreeOamMatrix(void *sprite) {
    // Stub: no-op
}

u8 AllocSpritePalette(u16 tag) {
    return 0;
}

// Sixth batch - battle cursor/controller/task globals
u8 gActionSelectionCursor[4] = {0};

u8 gMoveSelectionCursor[4] = {0};

void (*gBattleMainFunc)(void) = NULL;

void (**gBattlerControllerFuncs[4])(void) = {NULL, NULL, NULL, NULL};

struct Task {
    void (*func)(u8 taskId);
    u8 isActive;
    u8 prev;
    u8 next;
    u8 priority;
    s16 data[16];
};
struct Task gTasks[16] = {0};

u8 gBlockRecvBuffer[4][256] = {0};

// Seventh batch - battle state globals
u8 gUnusedFirstBattleVar2 = 0;

u8 gBattleOutcome = 0;

u16 gBattleWeather = 0;

struct {
    u8 field[64];
} gBattleScripting = {0};

void *gBattleStruct = NULL;

u8 gLinkBattleSendBuffer[256] = {0};

u8 gLinkBattleRecvBuffer[256] = {0};

// Eighth batch - more battle globals
u16 gChosenMove = 0;

u16 gLastUsedItem = 0;

u8 gLastUsedAbility = 0;

u8 gBattlerAttacker = 0;

u8 gBattlerTarget = 0;

u8 gEffectBattler = 0;

u8 gPotentialItemEffectBattler = 0;

u8 gAbsentBattlerFlags = 0;

// Ninth batch - battle controller and mon data
u8 gBattleControllerExecFlags = 0;

u8 gBattlersCount = 2;

u8 gBattlerPartyIndexes[4] = {0};

u8 gBattlerPositions[4] = {0};

struct BattlePokemon {
    u16 species;
    u16 attack;
    u16 defense;
    u16 speed;
    u16 spAttack;
    u16 spDefense;
    u16 moves[4];
    u32 hpIV:5;
    u32 attackIV:5;
    u32 defenseIV:5;
    u32 speedIV:5;
    u32 spAttackIV:5;
    u32 spDefenseIV:5;
    u32 isEgg:1;
    u32 abilityNum:1;
    s8 statStages[8];
    u8 ability;
    u8 type1;
    u8 type2;
    u8 unknown;
    u8 pp[4];
    u16 hp;
    u8 level;
    u8 friendship;
    u16 maxHP;
    u16 item;
    u8 nickname[11];
    u8 ppBonuses;
    u8 otName[11];
    u32 experience;
    u32 personality;
    u32 status1;
    u32 status2;
    u32 otId;
};
struct BattlePokemon gBattleMons[4] = {0};

u16 gCurrentMove = 0;

// Tenth batch - final battle globals
u8 gBattleTextBuff3[256] = {0};

u32 gBattleTypeFlags = 0;

u8 gUnusedFirstBattleVar1 = 0;

u8 gBattleBufferA[4][256] = {0};

u8 gBattleBufferB[4][256] = {0};

u8 gActiveBattler = 0;

// Eleventh batch - save data, party, and move data
struct SaveBlock2 {
    u8 playerName[8];
    u8 playerGender;
    u8 specialSaveWarpFlags;
    u8 playerTrainerId[4];
    u16 playTimeHours;
    u8 playTimeMinutes;
    u8 playTimeSeconds;
    u8 playTimeVBlanks;
    u8 optionsButtonMode;
    u16 optionsTextSpeed:3;
    u16 optionsWindowFrameType:5;
    u16 optionsSound:1;
    u16 optionsBattleStyle:1;
    u16 optionsBattleSceneOff:1;
    u16 regionMapZoom:1;
    u8 field[0x100];
};
struct SaveBlock2 gSaveBlock2 = {0};
struct SaveBlock2 *gSaveBlock2Ptr = &gSaveBlock2;

struct Pokemon {
    u32 personality;
    u32 otId;
    u8 nickname[10];
    u8 language;
    u8 otName[7];
    u8 markings;
    u16 checksum;
    u16 unknown;
    u8 data[48];
    u32 status;
    u8 level;
    u8 mail;
    u16 hp;
    u16 maxHP;
    u16 attack;
    u16 defense;
    u16 speed;
    u16 spAttack;
    u16 spDefense;
};
struct Pokemon gPlayerParty[6] = {0};

struct Pokemon gEnemyParty[6] = {0};

struct BattleMove {
    u8 effect;
    u8 power;
    u8 type;
    u8 accuracy;
    u8 pp;
    u8 secondaryEffectChance;
    u8 target;
    s8 priority;
    u8 flags;
    u8 split;
    u8 argument;
};
const struct BattleMove gBattleMoves[512] = {0};

u8 gBattleTextBuff1[256] = {0};

u8 gBattleTextBuff2[256] = {0};

// Twelfth batch - link and recorded battle functions
u8 IsLinkTaskFinished(void) {
    return 1;
}

void SetWirelessCommType1(void) {
    // Stub: no-op
}

u8 CheckShouldAdvanceLinkState(void) {
    return 1;
}

void DestroyTask_RfuIdle(void) {
    // Stub: no-op
}

void BufferBattlePartyCurrentOrderBySide(u8 *buffer, u8 side) {
    // Stub: no-op
}

void RecordedBattle_Init(void) {
    // Stub: no-op
}

void RecordedBattle_BufferNewBattlerData(u8 *data) {
    // Stub: no-op
}

void RecordedBattle_SaveParties(void) {
    // Stub: no-op
}

// Thirteenth batch - more link functions
u8 GetLinkPlayerCount(void) {
    return 1;
}

u8 GetMultiplayerId(void) {
    return 0;
}

u8 BitmaskAllOtherLinkPlayers(void) {
    return 0;
}

void SendBlock(u8 unused, const void *data, u16 size) {
    // Stub: no-op
}

u8 GetBlockReceivedStatus(void) {
    return 0;
}

void ResetBlockReceivedFlag(u8 who) {
    // Stub: no-op
}

u8 GetLinkPlayerCount_2(void) {
    return 1;
}

u8 IsLinkMaster(void) {
    return 1;
}

// Fourteenth batch - battle controller setup functions
void SetControllerToOpponent(void) {
    // Stub: no-op
}

void SetControllerToPlayerPartner(void) {
    // Stub: no-op
}

void SetControllerToSafari(void) {
    // Stub: no-op
}

void SetControllerToWally(void) {
    // Stub: no-op
}

void SetControllerToRecordedOpponent(void) {
    // Stub: no-op
}

void SetControllerToLinkOpponent(void) {
    // Stub: no-op
}

void SetControllerToLinkPartner(void) {
    // Stub: no-op
}

void Task_WaitForLinkPlayerConnection(u8 taskId) {
    // Stub: no-op
}

void OpenLink(void) {
    // Stub: no-op
}

// Fifteenth batch - final battle setup functions
void CreateMon(void *mon, u16 species, u8 level, u8 fixedIV, u8 hasFixedPersonality, u32 fixedPersonality, u8 otIdType, u32 fixedOtId) {
    // Stub: no-op
}

void ClearBattleMonForms(void) {
    // Stub: no-op
}

void BeginBattleIntroDummy(void) {
    // Stub: no-op
}

void BeginBattleIntro(void) {
    // Stub: no-op
}

void MarkBattlerReceivedLinkData(u8 battlerId) {
    // Stub: no-op
}

u8 AbilityBattleEffects(u8 caseID, u8 battlerId, u8 ability, u8 special, u16 moveArg) {
    return 0;
}

void BattleAI_HandleItemUseBeforeAISetup(u8 defaultScoreMoves) {
    // Stub: no-op
}

void ClearBattleAnimationVars(void) {
    // Stub: no-op
}

void SetControllerToRecordedPlayer(void) {
    // Stub: no-op
}

void ZeroEnemyPartyMons(void) {
    // Stub: no-op
}















