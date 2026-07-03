// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "ManifoldSlice.h"
#include "ManifoldTypes.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
#include "HarmonicsKernel.h"
#include "WavesKernel.h"
#include "RhythmKernel.h"
#include "GearsKernel.h"
#include "CircuitsKernel.h"
#include "RealmKernel.h"
#include "Serialization/MemoryReader.h"
#include "CorrespondenceSystem.h"
#include "TelemetrySystem.h"
#include "ManifoldGearMesh.h"
#include "ManifoldWaveMesh.h"
#include "ManifoldPalette.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/MemoryWriter.h"
#include "HAL/PlatformFileManager.h"

// End-to-end integration acceptance: proves the whole vertical-slice loop runs —
// two realms simulate, a correspondence is detected across the seam, power is
// transported, and the Insight Rate is positive (Build Plan §6 integration point).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSliceVerticalLoopTest, "MANIFOLD.Integration.VerticalSliceLoop", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSliceVerticalLoopTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    Slice->Setup(1111ULL, 2222ULL);

    FManifoldSliceResult Result = Slice->RunPlaythrough(30);

    UTEST_TRUE("Orbits resonance detected during playthrough", Result.bResonanceDetected);
    UTEST_TRUE("Fluids vortex detected during playthrough", Result.bVortexDetected);
    UTEST_GREATER("Correspondence ignited at least once", Result.CorrespondencesIgnited, 0);
    UTEST_GREATER("Transport completed at least once", Result.TransportsCompleted, 0);
    UTEST_GREATER("Insight Rate is positive", Result.InsightRate, 0.0f);

    return true;
}

// Constellation Lock (engine level): under the OctaveInvariant relation, two realms whose
// SURFACE ratios differ (3:1 vs 3:2) still correspond because they share an octave class,
// while a realm outside that class (5:4) does not — the precise discrimination the
// subset-hunt puzzle needs. The SAME three realms produce ZERO correspondences under
// Exact, proving the active relation genuinely changes what "corresponds" means.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstellationOctaveEngineTest, "MANIFOLD.Correspondence.ConstellationOctave", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FConstellationOctaveEngineTest::RunTest(const FString& Parameters)
{
    // Each "realm" is a Harmonics kernel whose two modes fix its surface ratio Hi:Lo.
    auto MakeRealm = [](int32 Lo, int32 Hi)
    {
        UHarmonicsKernel* K = NewObject<UHarmonicsKernel>();
        K->Initialize(1ULL);
        K->AddMode(static_cast<double>(Lo), 1.0);
        K->AddMode(static_cast<double>(Hi), 1.0);
        K->Step(0.01f);
        return K;
    };
    UHarmonicsKernel* RealmA = MakeRealm(1, 3); // surface 3:1
    UHarmonicsKernel* RealmB = MakeRealm(2, 3); // surface 3:2 (octave-equivalent to 3:1)
    UHarmonicsKernel* RealmC = MakeRealm(4, 5); // surface 5:4 (a different octave class)

    // --- Exact: 3:1, 3:2 and 5:4 are all literally different -> no correspondences. ---
    {
        UCorrespondenceSystem* Sys = NewObject<UCorrespondenceSystem>();
        Sys->RegisterRealm(TEXT("A"), TEXT("HarmonicRatio"), RealmA);
        Sys->RegisterRealm(TEXT("B"), TEXT("HarmonicRatio"), RealmB);
        Sys->RegisterRealm(TEXT("C"), TEXT("HarmonicRatio"), RealmC);
        Sys->SetActiveRelation(ECorrespondenceRelation::Exact);
        UTEST_EQUAL("Exact: surface-distinct realms do not correspond",
            Sys->DetectSharedStructureCorrespondences(), 0);
    }

    // --- OctaveInvariant: A(3:1) and B(3:2) collapse to the same class; C stays out. ---
    {
        UCorrespondenceSystem* Sys = NewObject<UCorrespondenceSystem>();
        int32 Discoveries = 0;
        TSet<FName> Partners;
        Sys->OnSharedStructureDiscovered.AddLambda(
            [&Discoveries, &Partners](FName A, FName B, FString /*Ratio*/, FGuid /*Id*/)
            {
                ++Discoveries;
                Partners.Add(A);
                Partners.Add(B);
            });
        Sys->RegisterRealm(TEXT("A"), TEXT("HarmonicRatio"), RealmA);
        Sys->RegisterRealm(TEXT("B"), TEXT("HarmonicRatio"), RealmB);
        Sys->RegisterRealm(TEXT("C"), TEXT("HarmonicRatio"), RealmC);
        Sys->SetActiveRelation(ECorrespondenceRelation::OctaveInvariant);

        UTEST_EQUAL("Octave: exactly one correspondence (A<->B)",
            Sys->DetectSharedStructureCorrespondences(), 1);
        UTEST_EQUAL("Octave: discovery fired exactly once", Discoveries, 1);
        UTEST_TRUE("Octave: the pair is A and B",
            Partners.Contains(FName(TEXT("A"))) && Partners.Contains(FName(TEXT("B"))));
        UTEST_FALSE("Octave: decoy C never corresponds", Partners.Contains(FName(TEXT("C"))));
    }

    return true;
}

// Constellation Lock (the player verb): the exact hidden subset scores and wins, igniting
// C(K,2) analogies; a plausible-but-wrong subset scores nothing and burns a probe.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstellationLockTest, "MANIFOLD.Play.ConstellationLock", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FConstellationLockTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    Slice->SetupConstellation(4242, 3);

    UTEST_TRUE("constellation mode active", Slice->IsConstellationMode());
    UTEST_EQUAL("six ratio realms are shown", Slice->GetConstellationRealmCount(), 6);

    const TArray<int32> Answer = Slice->GetConstellation();
    UTEST_EQUAL("K=3 hidden members", Answer.Num(), 3);

    // A plausible wrong guess: swap one true member for a non-member realm.
    TArray<int32> Wrong = Answer;
    for (int32 i = 0; i < 6; ++i) { if (!Answer.Contains(i)) { Wrong[0] = i; break; } }

    UTEST_FALSE("wrong lock does not score", Slice->PlayerLockConstellation(Wrong));
    UTEST_EQUAL("still in progress after a wrong lock",
        static_cast<int32>(Slice->GetSessionState()), static_cast<int32>(EManifoldSessionState::InProgress));
    UTEST_EQUAL("a probe was consumed", Slice->GetFailedProbes(), 1);
    UTEST_EQUAL("no discoveries from a wrong lock", Slice->GetTotalDiscoveries(), 0);

    // The exact hidden subset wins and ignites C(3,2)=3 cross-domain analogies.
    UTEST_TRUE("correct lock scores", Slice->PlayerLockConstellation(Answer));
    UTEST_EQUAL("session won",
        static_cast<int32>(Slice->GetSessionState()), static_cast<int32>(EManifoldSessionState::Won));
    UTEST_EQUAL("C(3,2)=3 discoveries ignited", Slice->GetTotalDiscoveries(), 3);

    return true;
}

// Constellation generation is deterministic in the seed and spreads across seeds (both
// relations occur; the hidden subset varies) — so no two sessions play the same, yet each
// reproduces exactly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstellationDeterminismTest, "MANIFOLD.Play.ConstellationDeterminism", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FConstellationDeterminismTest::RunTest(const FString& Parameters)
{
    const uint64 Seeds[] = { 1ULL, 7ULL, 100ULL, 99999ULL };
    for (uint64 S : Seeds)
    {
        UTEST_EQUAL("relation stable across calls",
            static_cast<int32>(UManifoldSlice::PickRelation(S)),
            static_cast<int32>(UManifoldSlice::PickRelation(S)));

        TArray<int32> A, B;
        UManifoldSlice::PickConstellation(S, 6, 3, A);
        UManifoldSlice::PickConstellation(S, 6, 3, B);
        UTEST_TRUE("constellation stable across calls", A == B);
        UTEST_EQUAL("selects exactly K", A.Num(), 3);
        for (int32 i = 1; i < A.Num(); ++i)
        {
            UTEST_TRUE("members sorted & distinct", A[i] > A[i - 1]);
        }
    }

    bool bSawExact = false, bSawOctave = false;
    TSet<FString> DistinctSubsets;
    for (uint64 S = 0; S < 200; ++S)
    {
        if (UManifoldSlice::PickRelation(S) == ECorrespondenceRelation::Exact) { bSawExact = true; }
        else { bSawOctave = true; }

        TArray<int32> M;
        UManifoldSlice::PickConstellation(S, 6, 3, M);
        DistinctSubsets.Add(FString::Printf(TEXT("%d,%d,%d"), M[0], M[1], M[2]));
    }
    UTEST_TRUE("both relations occur across seeds", bSawExact && bSawOctave);
    UTEST_GREATER("the hidden subset varies across seeds", DistinctSubsets.Num(), 5);

    return true;
}

// The heart of the mechanic under OctaveInvariant: the corresponding realms show
// DIFFERENT surface ratios (e.g. 3:1, 3:2, 6:1) yet collapse to ONE octave class — the
// player must normalize and reason, not eye-spot. The true subset still wins.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstellationOctaveSurfaceTest, "MANIFOLD.Play.ConstellationOctaveSurface", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FConstellationOctaveSurfaceTest::RunTest(const FString& Parameters)
{
    // Find a seed whose relation is Octave (both relations are common, so this is quick).
    uint64 Seed = 0;
    bool bFound = false;
    for (uint64 S = 0; S < 500; ++S)
    {
        if (UManifoldSlice::PickRelation(S) == ECorrespondenceRelation::OctaveInvariant)
        {
            Seed = S; bFound = true; break;
        }
    }
    UTEST_TRUE("found an octave-relation seed", bFound);

    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    Slice->SetupConstellation(static_cast<int64>(Seed), 3);
    UTEST_EQUAL("relation reported as Octave", Slice->GetActiveRelationName(), FString(TEXT("Octave")));

    const TArray<int32> Members = Slice->GetConstellation();
    UTEST_EQUAL("K=3 members", Members.Num(), 3);

    TSet<FString> Surfaces, Classes;
    for (int32 Idx : Members)
    {
        const FString Surface = Slice->GetRealmSurfaceRatio(Idx);
        Surfaces.Add(Surface);
        Classes.Add(UCorrespondenceSystem::NormalizeRatio(Surface, ECorrespondenceRelation::OctaveInvariant));
    }
    UTEST_GREATER("member surface ratios are not all identical", Surfaces.Num(), 1);
    UTEST_EQUAL("members share exactly one octave class", Classes.Num(), 1);

    UTEST_TRUE("the true subset wins", Slice->PlayerLockConstellation(Members));
    UTEST_EQUAL("won with C(3,2)=3 discoveries", Slice->GetTotalDiscoveries(), 3);

    return true;
}

// Robustness sweep across many seeds (both relations, every octave family, many subsets,
// K in {2,3}): the correct lock ALWAYS ignites exactly C(K,2) analogies and wins. This one
// test transitively proves (a) EVERY kernel realizes its assigned ratio (else discoveries
// fall short) and (b) decoys NEVER spuriously correspond (else discoveries overshoot). A
// single bad seed fails the test and names itself.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstellationRealizationSweepTest, "MANIFOLD.Play.ConstellationRealizationSweep", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FConstellationRealizationSweepTest::RunTest(const FString& Parameters)
{
    for (int32 Seed = 0; Seed < 96; ++Seed)
    {
        const int32 K = (Seed % 2 == 0) ? 3 : 2;
        const int32 Expected = K * (K - 1) / 2;

        UManifoldSlice* Slice = NewObject<UManifoldSlice>();
        Slice->SetupConstellation(static_cast<int64>(Seed), K);

        const TArray<int32> Answer = Slice->GetConstellation();
        if (!TestEqual(FString::Printf(TEXT("seed %d: %d hidden members"), Seed, K), Answer.Num(), K)) { return false; }

        // The six surface ratios must be REALIZED (no realm silently shows "-").
        for (int32 idx = 0; idx < 6; ++idx)
        {
            if (!TestNotEqual(FString::Printf(TEXT("seed %d: realm %d has a surface ratio"), Seed, idx),
                Slice->GetRealmSurfaceRatio(idx), FString(TEXT("-")))) { return false; }
        }

        if (!TestTrue(FString::Printf(TEXT("seed %d: correct lock wins"), Seed),
            Slice->PlayerLockConstellation(Answer))) { return false; }
        if (!TestEqual(FString::Printf(TEXT("seed %d: exactly C(K,2) discoveries"), Seed),
            Slice->GetTotalDiscoveries(), Expected)) { return false; }
        if (!TestEqual(FString::Printf(TEXT("seed %d: session Won"), Seed),
            static_cast<int32>(Slice->GetSessionState()), static_cast<int32>(EManifoldSessionState::Won))) { return false; }
    }
    return true;
}

// A Constellation-Lock session is a reproducible, shareable artifact: recorded from its
// seed + locked subset, it reproduces bit-for-bit on a fresh slice and survives a
// save/load round-trip through the versioned .manifoldreplay format.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstellationReplayTest, "MANIFOLD.Play.ConstellationReplay", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FConstellationReplayTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Recorder = NewObject<UManifoldSlice>();
    const FManifoldReplay Replay = Recorder->RecordConstellationReplay(555, 3);
    UTEST_EQUAL("recorded mode is constellation", static_cast<int32>(Replay.Mode), 1);
    UTEST_EQUAL("recorded C(3,2)=3 discoveries", Replay.FinalDiscoveries, 3);
    UTEST_EQUAL("recorded subset has K members", Replay.LockSelection.Num(), 3);

    // Reproduce on a fresh slice — same seed + subset => same win.
    UManifoldSlice* Player = NewObject<UManifoldSlice>();
    Player->RunReplay(Replay);
    UTEST_EQUAL("reproduced discoveries match record", Player->GetTotalDiscoveries(), Replay.FinalDiscoveries);
    UTEST_EQUAL("reproduced session won",
        static_cast<int32>(Player->GetSessionState()), static_cast<int32>(EManifoldSessionState::Won));

    // Persist + reload through the versioned file format.
    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TestConstellation.manifoldreplay"));
    UTEST_TRUE("save replay", UManifoldSlice::SaveReplay(Replay, Path));
    FManifoldReplay Loaded;
    UTEST_TRUE("load replay", UManifoldSlice::LoadReplay(Loaded, Path));
    UTEST_EQUAL("loaded mode is constellation", static_cast<int32>(Loaded.Mode), 1);
    UTEST_TRUE("loaded subset matches", Loaded.LockSelection == Replay.LockSelection);

    UManifoldSlice* Player2 = NewObject<UManifoldSlice>();
    Player2->RunReplay(Loaded);
    UTEST_EQUAL("loaded replay reproduces the win",
        static_cast<int32>(Player2->GetSessionState()), static_cast<int32>(EManifoldSessionState::Won));

    FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
    return true;
}

// Constellation scoring grades difficulty and precision so the rank is meaningful: an
// Octave solve (harder to infer) outscores an Exact solve, and a flawless solve outscores
// one that wasted a probe on the same puzzle.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstellationScoringTest, "MANIFOLD.Play.ConstellationScoring", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FConstellationScoringTest::RunTest(const FString& Parameters)
{
    int64 OctaveSeed = -1, ExactSeed = -1;
    for (int64 S = 0; S < 500 && (OctaveSeed < 0 || ExactSeed < 0); ++S)
    {
        const ECorrespondenceRelation R = UManifoldSlice::PickRelation(static_cast<uint64>(S));
        if (R == ECorrespondenceRelation::OctaveInvariant && OctaveSeed < 0) { OctaveSeed = S; }
        if (R == ECorrespondenceRelation::Exact && ExactSeed < 0) { ExactSeed = S; }
    }
    UTEST_TRUE("found an octave seed and an exact seed", OctaveSeed >= 0 && ExactSeed >= 0);

    // Flawless solves of each.
    UManifoldSlice* Oct = NewObject<UManifoldSlice>();
    Oct->SetupConstellation(OctaveSeed, 3);
    UTEST_TRUE("octave flawless win", Oct->PlayerLockConstellation(Oct->GetConstellation()));
    const int32 OctScore = Oct->GetScore();

    UManifoldSlice* Exa = NewObject<UManifoldSlice>();
    Exa->SetupConstellation(ExactSeed, 3);
    UTEST_TRUE("exact flawless win", Exa->PlayerLockConstellation(Exa->GetConstellation()));
    const int32 ExaScore = Exa->GetScore();

    UTEST_GREATER("octave (harder rule) outscores exact", OctScore, ExaScore);

    // Precision: a wasted probe on the SAME octave puzzle scores below the flawless run.
    UManifoldSlice* Probed = NewObject<UManifoldSlice>();
    Probed->SetupConstellation(OctaveSeed, 3);
    TArray<int32> Wrong = Probed->GetConstellation();
    for (int32 i = 0; i < 6; ++i) { if (!Probed->GetConstellation().Contains(i)) { Wrong[0] = i; break; } }
    UTEST_FALSE("a wrong lock", Probed->PlayerLockConstellation(Wrong));
    UTEST_TRUE("then the correct lock", Probed->PlayerLockConstellation(Probed->GetConstellation()));
    UTEST_EQUAL("probed run still wins",
        static_cast<int32>(Probed->GetSessionState()), static_cast<int32>(EManifoldSessionState::Won));
    UTEST_GREATER("flawless outscores a probed solve", OctScore, Probed->GetScore());

    return true;
}

// Expert Constellation hides the rule (the player must infer Exact vs Octave), which is
// the same puzzle as normal but worth more on a win — the design's full inference challenge.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstellationExpertTest, "MANIFOLD.Play.ConstellationExpert", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FConstellationExpertTest::RunTest(const FString& Parameters)
{
    // The SAME puzzle (same seed) solved normally and in expert (hidden-rule) mode.
    UManifoldSlice* Normal = NewObject<UManifoldSlice>();
    Normal->SetupConstellation(2024, 3, false);
    UTEST_FALSE("normal reveals the rule", Normal->IsRelationHintHidden());
    UTEST_TRUE("normal win", Normal->PlayerLockConstellation(Normal->GetConstellation()));
    const int32 NormalScore = Normal->GetScore();

    UManifoldSlice* Expert = NewObject<UManifoldSlice>();
    Expert->SetupConstellation(2024, 3, true);
    UTEST_TRUE("expert hides the rule", Expert->IsRelationHintHidden());
    UTEST_TRUE("expert is the same puzzle (rule only hidden)",
        Expert->GetConstellation() == Normal->GetConstellation());
    UTEST_TRUE("expert win", Expert->PlayerLockConstellation(Expert->GetConstellation()));

    UTEST_GREATER("expert (inferred the hidden rule) outscores normal",
        Expert->GetScore(), NormalScore);
    return true;
}

// Constellation locks give audible feedback with no display: a wrong lock buzzes, and a
// correct lock sounds the discovery chimes plus a bright victory fanfare (emitted last).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstellationAudioTest, "MANIFOLD.Play.ConstellationAudio", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FConstellationAudioTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* S = NewObject<UManifoldSlice>();
    S->SetupConstellation(321, 3);
    const int32 Base = S->GetAudioCueCount();

    TArray<int32> Wrong = S->GetConstellation();
    for (int32 i = 0; i < 6; ++i) { if (!S->GetConstellation().Contains(i)) { Wrong[0] = i; break; } }
    S->PlayerLockConstellation(Wrong);
    UTEST_EQUAL("a wrong lock emits one cue", S->GetAudioCueCount(), Base + 1);
    UTEST_EQUAL("the wrong-lock cue is a buzz",
        static_cast<int32>(S->GetLastAudioCue().Intent), static_cast<int32>(EManifoldCueIntent::FailureBuzz));

    const int32 AfterWrong = S->GetAudioCueCount();
    S->PlayerLockConstellation(S->GetConstellation());
    UTEST_GREATER("the correct lock emits discovery chimes + a victory", S->GetAudioCueCount(), AfterWrong + 1);
    UTEST_EQUAL("the last cue is the victory fanfare",
        static_cast<int32>(S->GetLastAudioCue().Intent), static_cast<int32>(EManifoldCueIntent::Victory));
    return true;
}

// A Constellation expedition strings subset-hunt puzzles into an escalating campaign with
// one cumulative score: a perfect run clears every level, it's deterministic in the base
// seed, and more levels accumulate more score (the difficulty ramp is real).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstellationExpeditionTest, "MANIFOLD.Play.ConstellationExpedition", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FConstellationExpeditionTest::RunTest(const FString& Parameters)
{
    const FManifoldExpeditionResult R = UManifoldSlice::RunConstellationExpedition(900, 5);
    UTEST_EQUAL("a perfect run clears all five levels", R.LevelsCleared, 5);
    UTEST_TRUE("the expedition completed", R.bCompleted);
    UTEST_GREATER("cumulative score is positive", R.TotalScore, 0);

    // Deterministic in the base seed.
    const FManifoldExpeditionResult R2 = UManifoldSlice::RunConstellationExpedition(900, 5);
    UTEST_EQUAL("deterministic levels cleared", R2.LevelsCleared, R.LevelsCleared);
    UTEST_EQUAL("deterministic cumulative score", R2.TotalScore, R.TotalScore);

    // More levels accumulate more score (each cleared level adds a positive amount).
    const FManifoldExpeditionResult R3 = UManifoldSlice::RunConstellationExpedition(900, 3);
    UTEST_GREATER("five levels outscore three", R.TotalScore, R3.TotalScore);
    return true;
}

// The two modes keep SEPARATE best scores (their scores aren't comparable), and both
// survive a profile save/load round-trip (format v2).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfilePerModeTest, "MANIFOLD.Play.ProfilePerMode", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FProfilePerModeTest::RunTest(const FString& Parameters)
{
    FManifoldProfile P;

    FManifoldSessionSummary Classic;
    Classic.State = EManifoldSessionState::Won;
    Classic.Score = 5000;
    Classic.bConstellation = false;
    UManifoldSlice::RecordSessionInProfile(P, Classic);
    UTEST_EQUAL("classic best recorded", P.BestScore, 5000);
    UTEST_EQUAL("constellation best untouched", P.BestConstellationScore, 0);

    FManifoldSessionSummary Const;
    Const.State = EManifoldSessionState::Won;
    Const.Score = 8000;
    Const.bConstellation = true;
    UManifoldSlice::RecordSessionInProfile(P, Const);
    UTEST_EQUAL("classic best unchanged by a constellation run", P.BestScore, 5000);
    UTEST_EQUAL("constellation best recorded separately", P.BestConstellationScore, 8000);
    UTEST_EQUAL("both sessions counted", P.SessionsPlayed, 2);
    UTEST_EQUAL("both wins counted", P.SessionsWon, 2);

    P.BestExpeditionScore = 12345; // a campaign best (format v3)

    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TestPerMode.manifoldprofile"));
    UTEST_TRUE("save profile", UManifoldSlice::SaveProfile(P, Path));
    FManifoldProfile L;
    UTEST_TRUE("load profile", UManifoldSlice::LoadProfile(L, Path));
    UTEST_EQUAL("loaded classic best", L.BestScore, 5000);
    UTEST_EQUAL("loaded constellation best", L.BestConstellationScore, 8000);
    UTEST_EQUAL("loaded expedition best", L.BestExpeditionScore, 12345);
    FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
    return true;
}

// Probe economy: paying to reveal a realm tells the truth about its membership (IN/out),
// is idempotent per realm, and costs score — so a revealed win scores below a clean one.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstellationRevealTest, "MANIFOLD.Play.ConstellationReveal", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FConstellationRevealTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* S = NewObject<UManifoldSlice>();
    S->SetupConstellation(77, 3);
    const TArray<int32> Ans = S->GetConstellation();

    const int32 Member = Ans[0];
    int32 NonMember = -1;
    for (int32 i = 0; i < 6; ++i) { if (!Ans.Contains(i)) { NonMember = i; break; } }
    UTEST_TRUE("found a non-member realm", NonMember >= 0);

    UTEST_EQUAL("an unrevealed realm reads -1 (unknown)", S->GetRevealedMembership(Member), -1);
    UTEST_TRUE("revealing a member returns true", S->PlayerRevealRealm(Member));
    UTEST_TRUE("member now marked revealed", S->IsRealmRevealed(Member));
    UTEST_EQUAL("revealed member reads IN (1)", S->GetRevealedMembership(Member), 1);

    UTEST_FALSE("revealing a non-member returns false", S->PlayerRevealRealm(NonMember));
    UTEST_EQUAL("revealed non-member reads out (0)", S->GetRevealedMembership(NonMember), 0);
    UTEST_EQUAL("two reveals were bought", S->GetRevealCount(), 2);

    // Idempotent: re-revealing the same realm adds no charge.
    S->PlayerRevealRealm(Member);
    UTEST_EQUAL("re-revealing is free", S->GetRevealCount(), 2);

    // Paid reveals cost score vs a clean solve of the same puzzle.
    UManifoldSlice* Clean = NewObject<UManifoldSlice>();
    Clean->SetupConstellation(77, 3);
    UTEST_TRUE("clean win", Clean->PlayerLockConstellation(Clean->GetConstellation()));
    const int32 CleanScore = Clean->GetScore();

    UTEST_TRUE("revealed win", S->PlayerLockConstellation(S->GetConstellation()));
    UTEST_GREATER("paid reveals reduce the final score", CleanScore, S->GetScore());
    return true;
}

// The shared-structure StableId must be CONTENT-stable (derived from the realm-id strings),
// not from process-local FName handles — otherwise the "deterministic id" contract breaks
// across runs/builds. Recompute the id from the string hashes and require an exact match.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSharedStructureStableIdTest, "MANIFOLD.Correspondence.StableIdContentStable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSharedStructureStableIdTest::RunTest(const FString& Parameters)
{
    auto MakeRealm = [](int32 Lo, int32 Hi)
    {
        UHarmonicsKernel* K = NewObject<UHarmonicsKernel>();
        K->Initialize(1ULL);
        K->AddMode(static_cast<double>(Lo), 1.0);
        K->AddMode(static_cast<double>(Hi), 1.0);
        K->Step(0.01f);
        return K;
    };
    UHarmonicsKernel* A = MakeRealm(2, 3); // 3:2
    UHarmonicsKernel* B = MakeRealm(2, 3); // 3:2

    UCorrespondenceSystem* Sys = NewObject<UCorrespondenceSystem>();
    FGuid Captured; FName GotA, GotB; FString GotRatio; bool bFired = false;
    Sys->OnSharedStructureDiscovered.AddLambda(
        [&Captured, &GotA, &GotB, &GotRatio, &bFired](FName a, FName b, FString r, FGuid id)
        {
            if (!bFired) { bFired = true; GotA = a; GotB = b; GotRatio = r; Captured = id; }
        });
    Sys->RegisterRealm(FName(TEXT("Alpha")), TEXT("HarmonicRatio"), A);
    Sys->RegisterRealm(FName(TEXT("Beta")),  TEXT("HarmonicRatio"), B);
    Sys->DetectSharedStructureCorrespondences();

    UTEST_TRUE("a shared structure fired", bFired);
    const uint32 hA = GetTypeHash(GotA.ToString());
    const uint32 hB = GetTypeHash(GotB.ToString());
    const uint32 hR = GetTypeHash(GotRatio);
    const FGuid Expected(hA ^ hB, hA + hB, hR, 0x5AA5u);
    UTEST_EQUAL("StableId is content-stable (string-hashed, not FName-hashed)", Captured, Expected);
    return true;
}

// Setup() must reset EVERY session field on reuse — including bAutoTransportOnIgnite, which
// RunReplay flips to false. Reusing a slice after a replay must still auto-transport.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSliceReuseFlagResetTest, "MANIFOLD.Play.SliceReuseResetsAutoTransport", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSliceReuseFlagResetTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();

    // RunReplay leaves bAutoTransportOnIgnite = false on this instance.
    const FManifoldReplay Rep = Slice->RecordReplay(1111, 2222, 30);
    Slice->RunReplay(Rep);

    // Reuse the SAME instance for a fresh classic run WITHOUT re-setting the flag.
    Slice->Setup(1111ULL, 2222ULL);
    const FManifoldSliceResult R = Slice->RunPlaythrough(30);
    UTEST_GREATER("Setup reset auto-transport, so a reused slice still transports",
        R.TransportsCompleted, 0);
    return true;
}

// RecordSessionInProfile reports whether a session set a NEW best for its mode (drives the
// "NEW BEST!" cue), tracking each mode's leaderboard independently.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNewBestDetectionTest, "MANIFOLD.Play.NewBestDetection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNewBestDetectionTest::RunTest(const FString& Parameters)
{
    FManifoldProfile P;

    FManifoldSessionSummary S1; S1.State = EManifoldSessionState::Won; S1.Score = 5000; S1.bConstellation = false;
    UTEST_TRUE("first classic score is a new best", UManifoldSlice::RecordSessionInProfile(P, S1));

    FManifoldSessionSummary S2; S2.State = EManifoldSessionState::Won; S2.Score = 3000; S2.bConstellation = false;
    UTEST_FALSE("a lower classic score is not a new best", UManifoldSlice::RecordSessionInProfile(P, S2));

    FManifoldSessionSummary S3; S3.State = EManifoldSessionState::Won; S3.Score = 6000; S3.bConstellation = false;
    UTEST_TRUE("a higher classic score is a new best", UManifoldSlice::RecordSessionInProfile(P, S3));
    UTEST_EQUAL("classic best updated to the new high", P.BestScore, 6000);

    // Constellation is a separate leaderboard — a small constellation score is still its first best.
    FManifoldSessionSummary C1; C1.State = EManifoldSessionState::Won; C1.Score = 100; C1.bConstellation = true;
    UTEST_TRUE("first constellation score is a new best (separate track)", UManifoldSlice::RecordSessionInProfile(P, C1));
    UTEST_EQUAL("classic best untouched by a constellation run", P.BestScore, 6000);
    UTEST_EQUAL("all four sessions counted", P.SessionsPlayed, 4);
    return true;
}

// Cross-feature integration: a player's career spanning BOTH modes accumulates onto one
// profile correctly — separate per-mode bests, combined session counts — and the whole
// profile survives a save/load round-trip. This composes classic play, constellation play,
// per-mode scoring, and persistence in one flow (no single unit test covers the mix).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMixedCareerProfileTest, "MANIFOLD.Integration.MixedCareerProfile", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMixedCareerProfileTest::RunTest(const FString& Parameters)
{
    FManifoldProfile Career;

    // --- A Classic win folds into the Classic leaderboard. ---
    UManifoldSlice* Classic = NewObject<UManifoldSlice>();
    Classic->Setup(1111ULL, 2222ULL);
    Classic->RunPlaythrough(30);
    const FManifoldSessionSummary ClassicSummary = Classic->GetSessionSummary();
    UTEST_EQUAL("classic session won",
        static_cast<int32>(ClassicSummary.State), static_cast<int32>(EManifoldSessionState::Won));
    UTEST_FALSE("classic summary not flagged constellation", ClassicSummary.bConstellation);
    UTEST_TRUE("first classic result is a new best",
        UManifoldSlice::RecordSessionInProfile(Career, ClassicSummary));
    UTEST_GREATER("classic best recorded", Career.BestScore, 0);
    UTEST_EQUAL("constellation best untouched by classic", Career.BestConstellationScore, 0);

    // --- A Constellation win folds into the SEPARATE Constellation leaderboard. ---
    UManifoldSlice* Const = NewObject<UManifoldSlice>();
    Const->SetupConstellation(4242, 3);
    UTEST_TRUE("constellation solved", Const->PlayerLockConstellation(Const->GetConstellation()));
    const FManifoldSessionSummary ConstSummary = Const->GetSessionSummary();
    UTEST_TRUE("constellation summary flagged", ConstSummary.bConstellation);
    UManifoldSlice::RecordSessionInProfile(Career, ConstSummary);
    UTEST_GREATER("constellation best recorded", Career.BestConstellationScore, 0);
    UTEST_EQUAL("classic best unchanged by a constellation win", Career.BestScore, ClassicSummary.Score);
    UTEST_EQUAL("both sessions counted", Career.SessionsPlayed, 2);
    UTEST_EQUAL("both wins counted", Career.SessionsWon, 2);

    // --- The whole career survives persistence. ---
    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TestCareer.manifoldprofile"));
    UTEST_TRUE("save career", UManifoldSlice::SaveProfile(Career, Path));
    FManifoldProfile Loaded;
    UTEST_TRUE("load career", UManifoldSlice::LoadProfile(Loaded, Path));
    UTEST_EQUAL("loaded classic best", Loaded.BestScore, Career.BestScore);
    UTEST_EQUAL("loaded constellation best", Loaded.BestConstellationScore, Career.BestConstellationScore);
    UTEST_EQUAL("loaded sessions won", Loaded.SessionsWon, 2);
    FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);

    return true;
}

// Procedural gear-cog geometry: a cog with N teeth is generated in code (no imported
// asset). The mesh is well-formed (indexed triangles, valid indices), scales with the
// tooth count (so the ratio integer is literally the number of teeth you see), clamps a
// degenerate 0-tooth request, and is deterministic.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGearMeshTest, "MANIFOLD.UI.GearMesh", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGearMeshTest::RunTest(const FString& Parameters)
{
    TArray<FVector> V3, V8; TArray<int32> T3, T8;
    ManifoldGearMesh::Build(3, 40.0f, 12.0f, 6.0f, V3, T3);
    ManifoldGearMesh::Build(8, 40.0f, 12.0f, 6.0f, V8, T8);

    UTEST_GREATER("3-tooth cog has vertices", V3.Num(), 0);
    UTEST_GREATER("3-tooth cog has triangles", T3.Num(), 0);
    UTEST_EQUAL("triangle list is whole triangles", T3.Num() % 3, 0);
    UTEST_EQUAL("vertex count is 2 + 4*Teeth (3)", V3.Num(), 2 + 4 * 3);
    UTEST_EQUAL("vertex count is 2 + 4*Teeth (8)", V8.Num(), 2 + 4 * 8);
    UTEST_GREATER("more teeth -> more geometry", V8.Num(), V3.Num());

    bool bInRange = true;
    for (int32 Idx : T8) { if (Idx < 0 || Idx >= V8.Num()) { bInRange = false; break; } }
    UTEST_TRUE("every triangle index references a real vertex", bInRange);

    // Deterministic: identical inputs produce identical geometry.
    TArray<FVector> V3b; TArray<int32> T3b;
    ManifoldGearMesh::Build(3, 40.0f, 12.0f, 6.0f, V3b, T3b);
    UTEST_TRUE("deterministic vertices", V3 == V3b);
    UTEST_TRUE("deterministic triangles", T3 == T3b);

    // A degenerate 0-tooth request clamps to a valid mesh (never empty/crash).
    TArray<FVector> V0; TArray<int32> T0;
    ManifoldGearMesh::Build(0, 40.0f, 12.0f, 6.0f, V0, T0);
    UTEST_GREATER("zero teeth clamps to a valid cog", V0.Num(), 0);

    return true;
}

// Balance curve: the difficulty tier a player takes on is reflected in a strictly higher
// rank for a flawless solve — Exact < Octave < Expert-Octave — and a flawless Expert-Octave
// solve earns the top rank. This locks the intended difficulty->rank design against silent
// regression (the verifiable half of tuning; felt pacing still wants a human).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstellationRankCurveTest, "MANIFOLD.Play.ConstellationRankCurve", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FConstellationRankCurveTest::RunTest(const FString& Parameters)
{
    int64 ExactSeed = -1, OctaveSeed = -1;
    for (int64 S = 0; S < 500 && (ExactSeed < 0 || OctaveSeed < 0); ++S)
    {
        const ECorrespondenceRelation R = UManifoldSlice::PickRelation(static_cast<uint64>(S));
        if (R == ECorrespondenceRelation::Exact && ExactSeed < 0) { ExactSeed = S; }
        if (R == ECorrespondenceRelation::OctaveInvariant && OctaveSeed < 0) { OctaveSeed = S; }
    }
    UTEST_TRUE("found an exact seed and an octave seed", ExactSeed >= 0 && OctaveSeed >= 0);

    auto FlawlessRank = [](int64 Seed, bool bExpert) -> EManifoldRank
    {
        UManifoldSlice* Slice = NewObject<UManifoldSlice>();
        Slice->SetupConstellation(Seed, 3, bExpert);
        Slice->PlayerLockConstellation(Slice->GetConstellation()); // flawless: no probes/reveals
        return Slice->GetRank();
    };

    const EManifoldRank ExactRank  = FlawlessRank(ExactSeed, false);
    const EManifoldRank OctaveRank = FlawlessRank(OctaveSeed, false);
    const EManifoldRank ExpertRank = FlawlessRank(OctaveSeed, true);

    UTEST_GREATER("octave flawless outranks exact flawless",
        static_cast<int32>(OctaveRank), static_cast<int32>(ExactRank));
    UTEST_GREATER("expert-octave flawless outranks plain octave",
        static_cast<int32>(ExpertRank), static_cast<int32>(OctaveRank));
    // Tuned mastery ladder (K=3): a flawless solve is always rewarded — Exact reaches B, Octave
    // reaches A, and Expert+Octave reaches the top rank S. Locks the flawless-bonus tuning so a
    // perfect solve of the harder mode is never out-ranked by a routine Classic ceiling run.
    UTEST_EQUAL("a flawless Exact solve earns B",
        static_cast<int32>(ExactRank), static_cast<int32>(EManifoldRank::B));
    UTEST_EQUAL("a flawless Octave solve earns A",
        static_cast<int32>(OctaveRank), static_cast<int32>(EManifoldRank::A));
    UTEST_EQUAL("a flawless expert-octave solve earns the top rank (S)",
        static_cast<int32>(ExpertRank), static_cast<int32>(EManifoldRank::S));
    return true;
}

// Procedural standing-wave ribbon: the N-th harmonic is drawn as sin(N*pi*t), so the
// number of humps is the harmonic (a 3-hump ribbon beside a 2-hump ribbon is the 3:2).
// Well-formed indexed mesh, scales with the harmonic, reaches +/-Amplitude, deterministic.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaveMeshTest, "MANIFOLD.UI.WaveMesh", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWaveMeshTest::RunTest(const FString& Parameters)
{
    TArray<FVector> V2, V5; TArray<int32> T2, T5;
    ManifoldWaveMesh::Build(2, 200.0f, 40.0f, 4.0f, V2, T2);
    ManifoldWaveMesh::Build(5, 200.0f, 40.0f, 4.0f, V5, T5);

    UTEST_GREATER("N=2 ribbon has vertices", V2.Num(), 0);
    UTEST_EQUAL("triangle list is whole triangles", T2.Num() % 3, 0);
    UTEST_EQUAL("vertex count = 2*(16*N+1) for N=2", V2.Num(), 2 * (16 * 2 + 1));
    UTEST_GREATER("higher harmonic -> more geometry", V5.Num(), V2.Num());

    bool bInRange = true;
    for (int32 Idx : T5) { if (Idx < 0 || Idx >= V5.Num()) { bInRange = false; break; } }
    UTEST_TRUE("every triangle index references a real vertex", bInRange);

    float MaxY = -1.0e9f, MinY = 1.0e9f;
    for (const FVector& P : V2) { MaxY = FMath::Max(MaxY, static_cast<float>(P.Y)); MinY = FMath::Min(MinY, static_cast<float>(P.Y)); }
    UTEST_TRUE("ribbon swings to +Amplitude", MaxY > 39.0f);
    UTEST_TRUE("ribbon swings to -Amplitude", MinY < -39.0f);

    TArray<FVector> V2b; TArray<int32> T2b;
    ManifoldWaveMesh::Build(2, 200.0f, 40.0f, 4.0f, V2b, T2b);
    UTEST_TRUE("deterministic vertices", V2 == V2b);
    UTEST_TRUE("deterministic triangles", T2 == T2b);

    TArray<FVector> V0; TArray<int32> T0;
    ManifoldWaveMesh::Build(0, 200.0f, 40.0f, 4.0f, V0, T0);
    UTEST_GREATER("zero harmonic clamps to a valid ribbon", V0.Num(), 0);

    return true;
}

// Every kernel's state must SERIALIZE -> DESERIALIZE faithfully (the IRealmKernel contract
// used by snapshots/replay): serialize a stepped kernel, load it into a FRESH kernel, and
// the recomputed state hash must match. A field-order or missing-field bug in any kernel's
// operator<< / SetState would surface here (this path had no test).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKernelSerializationRoundTripTest, "MANIFOLD.Systems.KernelSerializationRoundTrip", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKernelSerializationRoundTripTest::RunTest(const FString& Parameters)
{
    auto Check = [this](const TCHAR* Name, IRealmKernel* A, IRealmKernel* B) -> bool
    {
        TArray<uint8> Bytes;
        FMemoryWriter W(Bytes, /*bIsPersistent*/ true); A->SerializeState(W);
        FMemoryReader R(Bytes, /*bIsPersistent*/ true); B->DeserializeState(R);
        const uint64 HA = A->ComputeStateHash();
        const uint64 HB = B->ComputeStateHash();
        return TestTrue(FString::Printf(TEXT("%s produced non-trivial state"), Name), HA != 0)
            && TestEqual(FString::Printf(TEXT("%s round-trips (recomputed hash matches)"), Name),
                static_cast<int64>(HB), static_cast<int64>(HA));
    };

    {
        UHarmonicsKernel* A = NewObject<UHarmonicsKernel>(); A->Initialize(7ULL); A->AddMode(2.0, 1.0); A->AddMode(3.0, 1.0); A->Step(0.01f);
        UHarmonicsKernel* B = NewObject<UHarmonicsKernel>(); B->Initialize(111ULL);
        if (!Check(TEXT("Harmonics"), A, B)) { return false; }
    }
    {
        UWavesKernel* A = NewObject<UWavesKernel>(); A->Initialize(7ULL); A->ExciteHarmonic(2, 1.0); A->ExciteHarmonic(3, 1.0); A->Step(0.001f);
        UWavesKernel* B = NewObject<UWavesKernel>(); B->Initialize(111ULL);
        if (!Check(TEXT("Waves"), A, B)) { return false; }
    }
    {
        URhythmKernel* A = NewObject<URhythmKernel>(); A->Initialize(7ULL); A->AddVoice(3.0); A->AddVoice(2.0); A->Step(0.016f);
        URhythmKernel* B = NewObject<URhythmKernel>(); B->Initialize(111ULL);
        if (!Check(TEXT("Rhythm"), A, B)) { return false; }
    }
    {
        UGearsKernel* A = NewObject<UGearsKernel>(); A->Initialize(7ULL); A->AddGear(3); A->AddGear(2); A->Step(0.016f);
        UGearsKernel* B = NewObject<UGearsKernel>(); B->Initialize(111ULL);
        if (!Check(TEXT("Gears"), A, B)) { return false; }
    }
    {
        UCircuitsKernel* A = NewObject<UCircuitsKernel>(); A->Initialize(7ULL); A->AddTank(2.0, 1.0); A->AddTank(3.0, 1.0); A->Step(0.016f);
        UCircuitsKernel* B = NewObject<UCircuitsKernel>(); B->Initialize(111ULL);
        if (!Check(TEXT("Circuits"), A, B)) { return false; }
    }
    {
        UOrbitsKernel* A = NewObject<UOrbitsKernel>(); A->Initialize(7ULL);
        FOrbitsBodyDef Star; Star.Mass = 1.989e30; Star.bIsCentral = true; A->AddBody(Star);
        FOrbitsBodyDef P; P.Mass = 1e24; P.Position = FVector(1.496e11, 0.0, 0.0);
        P.Velocity = FVector(0.0, FMath::Sqrt((A->G * Star.Mass) / P.Position.X), 0.0); A->AddBody(P);
        A->Step(0.1f);
        UOrbitsKernel* B = NewObject<UOrbitsKernel>(); B->Initialize(111ULL);
        if (!Check(TEXT("Orbits"), A, B)) { return false; }
    }
    {
        UFluidsKernel* A = NewObject<UFluidsKernel>(); A->Initialize(7ULL);
        A->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f); A->AddVelocity(0.55f, 0.5f, 0.0f, 20.0f); A->Step(0.016f);
        UFluidsKernel* B = NewObject<UFluidsKernel>(); B->Initialize(111ULL);
        if (!Check(TEXT("Fluids"), A, B)) { return false; }
    }
    return true;
}

// The IRealmKernel contract says StepMultiple(dt, N) must equal N calls to Step(dt). A bad
// per-kernel override would silently diverge; this locks the equivalence for all 7 realms.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKernelStepMultipleEquivalenceTest, "MANIFOLD.Systems.KernelStepMultipleEquivalence", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKernelStepMultipleEquivalenceTest::RunTest(const FString& Parameters)
{
    auto Check = [this](const TCHAR* Name, IRealmKernel* Multi, IRealmKernel* Looped, int32 N, float Dt) -> bool
    {
        Multi->StepMultiple(Dt, N);
        for (int32 i = 0; i < N; ++i) { Looped->Step(Dt); }
        return TestEqual(FString::Printf(TEXT("%s: StepMultiple(dt,N) == N x Step(dt)"), Name),
            static_cast<int64>(Multi->ComputeStateHash()), static_cast<int64>(Looped->ComputeStateHash()));
    };

    {
        UHarmonicsKernel* M = NewObject<UHarmonicsKernel>(); M->Initialize(7ULL); M->AddMode(2.0, 1.0); M->AddMode(3.0, 1.0);
        UHarmonicsKernel* L = NewObject<UHarmonicsKernel>(); L->Initialize(7ULL); L->AddMode(2.0, 1.0); L->AddMode(3.0, 1.0);
        if (!Check(TEXT("Harmonics"), M, L, 10, 0.01f)) { return false; }
    }
    {
        UWavesKernel* M = NewObject<UWavesKernel>(); M->Initialize(7ULL); M->ExciteHarmonic(2, 1.0); M->ExciteHarmonic(3, 1.0);
        UWavesKernel* L = NewObject<UWavesKernel>(); L->Initialize(7ULL); L->ExciteHarmonic(2, 1.0); L->ExciteHarmonic(3, 1.0);
        if (!Check(TEXT("Waves"), M, L, 10, 0.001f)) { return false; }
    }
    {
        URhythmKernel* M = NewObject<URhythmKernel>(); M->Initialize(7ULL); M->AddVoice(3.0); M->AddVoice(2.0);
        URhythmKernel* L = NewObject<URhythmKernel>(); L->Initialize(7ULL); L->AddVoice(3.0); L->AddVoice(2.0);
        if (!Check(TEXT("Rhythm"), M, L, 10, 0.016f)) { return false; }
    }
    {
        UGearsKernel* M = NewObject<UGearsKernel>(); M->Initialize(7ULL); M->AddGear(3); M->AddGear(2);
        UGearsKernel* L = NewObject<UGearsKernel>(); L->Initialize(7ULL); L->AddGear(3); L->AddGear(2);
        if (!Check(TEXT("Gears"), M, L, 10, 0.016f)) { return false; }
    }
    {
        UCircuitsKernel* M = NewObject<UCircuitsKernel>(); M->Initialize(7ULL); M->AddTank(2.0, 1.0); M->AddTank(3.0, 1.0);
        UCircuitsKernel* L = NewObject<UCircuitsKernel>(); L->Initialize(7ULL); L->AddTank(2.0, 1.0); L->AddTank(3.0, 1.0);
        if (!Check(TEXT("Circuits"), M, L, 10, 0.016f)) { return false; }
    }
    {
        auto Setup = [](UOrbitsKernel* K)
        {
            FOrbitsBodyDef Star; Star.Mass = 1.989e30; Star.bIsCentral = true; K->AddBody(Star);
            FOrbitsBodyDef P; P.Mass = 1e24; P.Position = FVector(1.496e11, 0.0, 0.0);
            P.Velocity = FVector(0.0, FMath::Sqrt((K->G * Star.Mass) / P.Position.X), 0.0); K->AddBody(P);
        };
        UOrbitsKernel* M = NewObject<UOrbitsKernel>(); M->Initialize(7ULL); Setup(M);
        UOrbitsKernel* L = NewObject<UOrbitsKernel>(); L->Initialize(7ULL); Setup(L);
        if (!Check(TEXT("Orbits"), M, L, 10, 0.1f)) { return false; }
    }
    {
        auto Setup = [](UFluidsKernel* K)
        {
            K->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f); K->AddVelocity(0.55f, 0.5f, 0.0f, 20.0f);
        };
        UFluidsKernel* M = NewObject<UFluidsKernel>(); M->Initialize(7ULL); Setup(M);
        UFluidsKernel* L = NewObject<UFluidsKernel>(); L->Initialize(7ULL); Setup(L);
        if (!Check(TEXT("Fluids"), M, L, 10, 0.016f)) { return false; }
    }
    return true;
}

// Reset() must return a kernel to a clean, reproducible post-init state: after Reset, redoing
// the same structures + steps must reproduce the same state hash (no leftover state leaks).
// This completes IRealmKernel contract coverage across all 7 realms.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKernelResetTest, "MANIFOLD.Systems.KernelReset", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKernelResetTest::RunTest(const FString& Parameters)
{
    {
        UHarmonicsKernel* K = NewObject<UHarmonicsKernel>(); K->Initialize(7ULL);
        K->AddMode(2.0, 1.0); K->AddMode(3.0, 1.0); K->Step(0.01f);
        const uint64 H1 = K->ComputeStateHash();
        K->Reset(); K->AddMode(2.0, 1.0); K->AddMode(3.0, 1.0); K->Step(0.01f);
        if (!TestEqual(TEXT("Harmonics reset reproduces"), static_cast<int64>(K->ComputeStateHash()), static_cast<int64>(H1))) { return false; }
    }
    {
        UWavesKernel* K = NewObject<UWavesKernel>(); K->Initialize(7ULL);
        K->ExciteHarmonic(2, 1.0); K->ExciteHarmonic(3, 1.0); K->Step(0.001f);
        const uint64 H1 = K->ComputeStateHash();
        K->Reset(); K->ExciteHarmonic(2, 1.0); K->ExciteHarmonic(3, 1.0); K->Step(0.001f);
        if (!TestEqual(TEXT("Waves reset reproduces"), static_cast<int64>(K->ComputeStateHash()), static_cast<int64>(H1))) { return false; }
    }
    {
        URhythmKernel* K = NewObject<URhythmKernel>(); K->Initialize(7ULL);
        K->AddVoice(3.0); K->AddVoice(2.0); K->Step(0.016f);
        const uint64 H1 = K->ComputeStateHash();
        K->Reset(); K->AddVoice(3.0); K->AddVoice(2.0); K->Step(0.016f);
        if (!TestEqual(TEXT("Rhythm reset reproduces"), static_cast<int64>(K->ComputeStateHash()), static_cast<int64>(H1))) { return false; }
    }
    {
        UGearsKernel* K = NewObject<UGearsKernel>(); K->Initialize(7ULL);
        K->AddGear(3); K->AddGear(2); K->Step(0.016f);
        const uint64 H1 = K->ComputeStateHash();
        K->Reset(); K->AddGear(3); K->AddGear(2); K->Step(0.016f);
        if (!TestEqual(TEXT("Gears reset reproduces"), static_cast<int64>(K->ComputeStateHash()), static_cast<int64>(H1))) { return false; }
    }
    {
        UCircuitsKernel* K = NewObject<UCircuitsKernel>(); K->Initialize(7ULL);
        K->AddTank(2.0, 1.0); K->AddTank(3.0, 1.0); K->Step(0.016f);
        const uint64 H1 = K->ComputeStateHash();
        K->Reset(); K->AddTank(2.0, 1.0); K->AddTank(3.0, 1.0); K->Step(0.016f);
        if (!TestEqual(TEXT("Circuits reset reproduces"), static_cast<int64>(K->ComputeStateHash()), static_cast<int64>(H1))) { return false; }
    }
    {
        auto Setup = [](UOrbitsKernel* K)
        {
            FOrbitsBodyDef Star; Star.Mass = 1.989e30; Star.bIsCentral = true; K->AddBody(Star);
            FOrbitsBodyDef P; P.Mass = 1e24; P.Position = FVector(1.496e11, 0.0, 0.0);
            P.Velocity = FVector(0.0, FMath::Sqrt((K->G * Star.Mass) / P.Position.X), 0.0); K->AddBody(P);
        };
        UOrbitsKernel* K = NewObject<UOrbitsKernel>(); K->Initialize(7ULL); Setup(K); K->Step(0.1f);
        const uint64 H1 = K->ComputeStateHash();
        K->Reset(); Setup(K); K->Step(0.1f);
        if (!TestEqual(TEXT("Orbits reset reproduces"), static_cast<int64>(K->ComputeStateHash()), static_cast<int64>(H1))) { return false; }
    }
    {
        auto Setup = [](UFluidsKernel* K) { K->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f); K->AddVelocity(0.55f, 0.5f, 0.0f, 20.0f); };
        UFluidsKernel* K = NewObject<UFluidsKernel>(); K->Initialize(7ULL); Setup(K); K->Step(0.016f);
        const uint64 H1 = K->ComputeStateHash();
        K->Reset(); Setup(K); K->Step(0.016f);
        if (!TestEqual(TEXT("Fluids reset reproduces"), static_cast<int64>(K->ComputeStateHash()), static_cast<int64>(H1))) { return false; }
    }
    return true;
}

// Control build (Build Plan D3): with NO correspondence content the loop must NOT
// manufacture insight — the moat is that unsolved seams stay unsolved. Here we run
// only the Fluids realm (no resonance to correspond with), so nothing ignites.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSliceControlNoCorrespondenceTest, "MANIFOLD.Integration.ControlNoCorrespondence", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSliceControlNoCorrespondenceTest::RunTest(const FString& Parameters)
{
    // Build a control slice by hand with only ONE planet: a single body yields no
    // body-body resonance, so the seam has nothing to match and nothing ignites.
    UManifoldSlice* Control = NewObject<UManifoldSlice>();
    Control->Orbits = NewObject<UOrbitsKernel>(Control);
    Control->Fluids = NewObject<UFluidsKernel>(Control);
    Control->Correspond = NewObject<UCorrespondenceSystem>(Control);
    Control->Telemetry = NewObject<UTelemetrySystem>(Control);
    Control->Orbits->Initialize(1111ULL);
    Control->Fluids->Initialize(2222ULL);
    Control->Correspond->RegisterKernels(Control->Orbits, Control->Fluids);
    Control->Telemetry->InitializeTelemetry(TEXT("ControlPlaythrough.log"));

    FOrbitsBodyDef Star; Star.Name = TEXT("Star"); Star.Mass = 1.989e30; Star.bIsCentral = true;
    Control->Orbits->AddBody(Star);
    FOrbitsBodyDef Planet; Planet.Name = TEXT("Solo"); Planet.Mass = 1e24; Planet.Position = FVector(1.496e11, 0.0, 0.0);
    const double V = FMath::Sqrt((Control->Orbits->G * Star.Mass) / Planet.Position.X);
    Planet.Velocity = FVector(0.0, V, 0.0);
    Control->Orbits->AddBody(Planet);
    Control->Fluids->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f);

    for (int32 i = 0; i < 30; ++i)
    {
        Control->Orbits->Step(0.1f);
        Control->Fluids->Step(0.016f);
        Control->Correspond->DetectCorrespondence();
    }

    UTEST_EQUAL("Control: no resonance with a single planet", Control->Orbits->GetActiveResonances().Num(), 0);
    UTEST_EQUAL("Control: Insight Rate stays zero without a correspondence", Control->Telemetry->CalculateInsightRate(), 0.0f);

    return true;
}

// The core mechanic, generalized to N realms: ANY two realms exposing the same
// structure ratio correspond. Here Orbits (3:2 mean-motion resonance) and Harmonics
// (3:2 frequency ratio) share the "3:2" structure across totally different domains —
// the cross-domain analogy that is the heart of MANIFOLD.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMultiRealmCorrespondenceTest, "MANIFOLD.Integration.MultiRealmCorrespondence", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMultiRealmCorrespondenceTest::RunTest(const FString& Parameters)
{
    // Orbits with a 3:2 mean-motion resonance.
    UOrbitsKernel* Orbits = NewObject<UOrbitsKernel>();
    Orbits->Initialize(1111ULL);
    FOrbitsBodyDef Star; Star.Mass = 1.989e30; Star.bIsCentral = true; Orbits->AddBody(Star);
    FOrbitsBodyDef A; A.Mass = 1e24; A.Position = FVector(1.496e11, 0.0, 0.0);
    const double vA = FMath::Sqrt((Orbits->G * Star.Mass) / A.Position.X); A.Velocity = FVector(0.0, vA, 0.0);
    Orbits->AddBody(A);
    const double rB = A.Position.X * FMath::Pow(1.5, 2.0 / 3.0);
    FOrbitsBodyDef B; B.Mass = 1e24; B.Position = FVector(rB, 0.0, 0.0);
    const double vB = FMath::Sqrt((Orbits->G * Star.Mass) / rB); B.Velocity = FVector(0.0, vB, 0.0);
    Orbits->AddBody(B);
    Orbits->Step(0.1f);

    // Harmonics with a 3:2 frequency ratio (2 Hz vs 3 Hz).
    UHarmonicsKernel* Harmonics = NewObject<UHarmonicsKernel>();
    Harmonics->Initialize(999ULL);
    Harmonics->AddMode(2.0, 1.0);
    Harmonics->AddMode(3.0, 1.0);
    Harmonics->Step(0.01f);

    UCorrespondenceSystem* Correspond = NewObject<UCorrespondenceSystem>();
    Correspond->RegisterRealm(TEXT("Orbits"), TEXT("OrbitalResonance"), Orbits);
    Correspond->RegisterRealm(TEXT("Harmonics"), TEXT("HarmonicRatio"), Harmonics);

    bool bDiscovered = false;
    FString DiscoveredRatio;
    Correspond->OnSharedStructureDiscovered.AddLambda(
        [&bDiscovered, &DiscoveredRatio](FName, FName, FString Ratio, FGuid Id)
        {
            bDiscovered = true;
            DiscoveredRatio = Ratio;
            // The shared-structure id must be real (stable), not a throwaway zero/GUID.
            check(Id.IsValid());
        });

    const int32 Found = Correspond->DetectSharedStructureCorrespondences();
    UTEST_GREATER("Shared 3:2 structure found across Orbits and Harmonics", Found, 0);
    UTEST_TRUE("Cross-domain correspondence discovered", bDiscovered);
    UTEST_EQUAL("Discovery carries the real shared ratio", DiscoveredRatio, FString(TEXT("3:2")));

    // Idempotent: the same shared structure does not re-ignite.
    UTEST_EQUAL("No duplicate ignition on re-detect", Correspond->DetectSharedStructureCorrespondences(), 0);

    return true;
}

// The playable-session logic behind the interactive shell: tick the slice over
// time, the correspondence lights up (but does NOT auto-transport), then the
// PLAYER'S verb carries it across the seam. This is the loop a human will feel.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractiveSessionTest, "MANIFOLD.Play.InteractiveSession", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FInteractiveSessionTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    Slice->Setup(1111ULL, 2222ULL);
    Slice->bAutoTransportOnIgnite = false; // interactive: player triggers transport (set after Setup)

    for (int32 i = 0; i < 30; ++i)
    {
        Slice->Tick();
    }

    UTEST_TRUE("Resonance detected", Slice->HasResonance());
    UTEST_TRUE("Vortex detected", Slice->HasVortex());
    // All ratio realms realize this session's hidden ratio (whatever the seed chose).
    UTEST_EQUAL("Orbits realizes the hidden ratio", Slice->GetOrbitsRatio(), Slice->GetSharedRatio());
    UTEST_EQUAL("Harmonics realizes the hidden ratio", Slice->GetHarmonicsRatio(), Slice->GetSharedRatio());
    UTEST_GREATER("Cross-domain analogy (Orbits<->Harmonics) discovered", Slice->GetSharedDiscoveries(), 0);
    UTEST_GREATER("Correspondence lit at least once", Slice->GetCorrespondencesIgnited(), 0);
    UTEST_GREATER("Insight Rate positive", Slice->GetInsightRate(), 0.0f);
    UTEST_TRUE("Correspondence available to the player", Slice->IsCorrespondenceAvailable());
    UTEST_EQUAL("No auto-transport in interactive mode", Slice->GetTransportsCompleted(), 0);

    // The player's verb.
    UTEST_TRUE("Player transport succeeds when a correspondence is lit", Slice->PlayerRequestTransport());
    UTEST_GREATER("Transport recorded", Slice->GetTransportsCompleted(), 0);

    return true;
}

// Vertical-slice GATE (Build Plan §5 Stream P / P2): the numeric go/no-go. The
// slice with a correspondence must produce a positive Insight Rate; the control
// (no correspondence) must produce exactly zero — and the slice must strictly beat
// the control. This is the "prove the loop compounds vs. control" criterion in code.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVerticalSliceGateTest, "MANIFOLD.Play.VerticalSliceGate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FVerticalSliceGateTest::RunTest(const FString& Parameters)
{
    // Treatment: the full slice.
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    Slice->Setup(1111ULL, 2222ULL);
    const FManifoldSliceResult Treatment = Slice->RunPlaythrough(30);

    // Control: a single-planet Orbits + Fluids, no resonance -> no correspondence.
    UManifoldSlice* Control = NewObject<UManifoldSlice>();
    Control->Orbits = NewObject<UOrbitsKernel>(Control);
    Control->Fluids = NewObject<UFluidsKernel>(Control);
    Control->Correspond = NewObject<UCorrespondenceSystem>(Control);
    Control->Telemetry = NewObject<UTelemetrySystem>(Control);
    Control->Orbits->Initialize(1111ULL);
    Control->Fluids->Initialize(2222ULL);
    Control->Correspond->RegisterKernels(Control->Orbits, Control->Fluids);
    Control->Telemetry->InitializeTelemetry(TEXT("GateControl.log"));
    FOrbitsBodyDef Star; Star.Mass = 1.989e30; Star.bIsCentral = true; Control->Orbits->AddBody(Star);
    FOrbitsBodyDef Planet; Planet.Mass = 1e24; Planet.Position = FVector(1.496e11, 0.0, 0.0);
    const double V = FMath::Sqrt((Control->Orbits->G * Star.Mass) / Planet.Position.X);
    Planet.Velocity = FVector(0.0, V, 0.0);
    Control->Orbits->AddBody(Planet);
    Control->Fluids->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f);
    for (int32 i = 0; i < 30; ++i)
    {
        Control->Orbits->Step(0.1f);
        Control->Fluids->Step(0.016f);
        Control->Correspond->DetectCorrespondence();
    }
    const float ControlInsight = Control->Telemetry->CalculateInsightRate();

    UTEST_GREATER("GATE: treatment Insight Rate is positive", Treatment.InsightRate, 0.0f);
    UTEST_EQUAL("GATE: control Insight Rate is zero", ControlInsight, 0.0f);
    UTEST_GREATER("GATE: treatment strictly beats control (loop compounds)", Treatment.InsightRate, ControlInsight);

    return true;
}

// Objective — WIN: a session with a reachable target resolves to Won once the
// player has surfaced enough discoveries. This is the goal that turns the endless
// simulation into a game with a finish line.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectiveWinTest, "MANIFOLD.Play.ObjectiveWin", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectiveWinTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    FManifoldObjective Obj;
    Obj.TargetDiscoveries = 1; // one discovery is enough to win
    Obj.StepBudget = 0;        // unlimited — cannot lose
    Slice->SetObjective(Obj);
    Slice->Setup(1111ULL, 2222ULL);

    Slice->RunPlaythrough(30);

    UTEST_GREATER("At least one discovery surfaced", Slice->GetTotalDiscoveries(), 0);
    UTEST_EQUAL("Session resolved to Won", (int32)Slice->GetSessionState(), (int32)EManifoldSessionState::Won);

    const FManifoldSessionSummary Summary = Slice->GetSessionSummary();
    UTEST_EQUAL("Summary state is Won", (int32)Summary.State, (int32)EManifoldSessionState::Won);
    UTEST_GREATER("Summary reports discoveries", Summary.Discoveries, 0);

    return true;
}

// Objective — LOSE: an unreachable target within a tight step budget resolves to
// Lost when the budget runs out. Proves the moat holds — you cannot manufacture a
// win the simulation did not earn.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectiveLoseTest, "MANIFOLD.Play.ObjectiveLose", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectiveLoseTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    FManifoldObjective Obj;
    Obj.TargetDiscoveries = 999; // never reachable
    Obj.StepBudget = 5;          // and the clock runs out fast
    Slice->SetObjective(Obj);
    Slice->Setup(1111ULL, 2222ULL);

    Slice->RunPlaythrough(10); // more than the budget

    UTEST_EQUAL("Session resolved to Lost", (int32)Slice->GetSessionState(), (int32)EManifoldSessionState::Lost);

    return true;
}

// Determinism: the SAME seeds produce a bit-for-bit identical session — same
// discovery count, same transport schedule, same Insight Rate. This is the
// "un-pre-computable yet perfectly reproducible" pillar, tested at the whole-slice
// level (not just per-kernel).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSliceDeterminismTest, "MANIFOLD.Play.SliceDeterminism", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSliceDeterminismTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* A = NewObject<UManifoldSlice>();
    UManifoldSlice* B = NewObject<UManifoldSlice>();
    const FManifoldReplay RA = A->RecordReplay(4242LL, 7777LL, 40);
    const FManifoldReplay RB = B->RecordReplay(4242LL, 7777LL, 40);

    UTEST_EQUAL("Same discoveries", RA.FinalDiscoveries, RB.FinalDiscoveries);
    UTEST_EQUAL("Same transports", RA.FinalTransports, RB.FinalTransports);
    UTEST_EQUAL("Same Insight Rate (bit-identical)", RA.FinalInsightRate, RB.FinalInsightRate);
    UTEST_TRUE("Same transport schedule", RA.TransportSteps == RB.TransportSteps);

    return true;
}

// Replay round-trip: record a session, reproduce it on a fresh slice (player firing
// the verb on the recorded schedule) and get the same result; then persist the
// recording to disk, load it back, and reproduce again identically.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReplayRoundTripTest, "MANIFOLD.Play.ReplayRoundTrip", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FReplayRoundTripTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Recorder = NewObject<UManifoldSlice>();
    const FManifoldReplay Rec = Recorder->RecordReplay(1111LL, 2222LL, 30);
    UTEST_GREATER("Recording captured a transport", Rec.FinalTransports, 0);

    // Reproduce from the in-memory recording.
    UManifoldSlice* Player = NewObject<UManifoldSlice>();
    const FManifoldSliceResult Reproduced = Player->RunReplay(Rec);
    UTEST_EQUAL("Reproduced transports match", Reproduced.TransportsCompleted, Rec.FinalTransports);
    UTEST_EQUAL("Reproduced Insight Rate matches", Reproduced.InsightRate, Rec.FinalInsightRate);
    UTEST_TRUE("Reproduced transport schedule matches recording", Player->GetTransportSchedule() == Rec.TransportSteps);

    // Persist to disk and reload.
    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RoundTrip.manifoldreplay"));
    UTEST_TRUE("Replay saved to disk", UManifoldSlice::SaveReplay(Rec, Path));

    FManifoldReplay Loaded;
    UTEST_TRUE("Replay loaded from disk", UManifoldSlice::LoadReplay(Loaded, Path));
    UTEST_EQUAL("Loaded seeds preserved (orbits)", (int64)Loaded.OrbitsSeed, (int64)Rec.OrbitsSeed);
    UTEST_EQUAL("Loaded schedule preserved", Loaded.TransportSteps.Num(), Rec.TransportSteps.Num());
    UTEST_TRUE("Loaded schedule identical", Loaded.TransportSteps == Rec.TransportSteps);

    // Reproduce from the loaded recording — still identical.
    UManifoldSlice* Player2 = NewObject<UManifoldSlice>();
    const FManifoldSliceResult FromDisk = Player2->RunReplay(Loaded);
    UTEST_EQUAL("Disk-replay transports match", FromDisk.TransportsCompleted, Rec.FinalTransports);
    UTEST_EQUAL("Disk-replay Insight Rate matches", FromDisk.InsightRate, Rec.FinalInsightRate);

    // Clean up the scratch file.
    FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);

    return true;
}

// Generic engine, generalized: a realm can expose MULTIPLE structure ratios, and two
// realms correspond on EVERY shared ratio (not just each realm's strongest). Proves the
// QueryAll path + all-vs-all matching. A 3-mode Harmonics realm exposes 3 ratios.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueryAllMultiRatioTest, "MANIFOLD.Integration.QueryAllMultiRatio", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FQueryAllMultiRatioTest::RunTest(const FString& Parameters)
{
    // Modes 2,3,4 -> ratios 3:2, 2:1, 4:3 (three distinct structures in one realm).
    UHarmonicsKernel* A = NewObject<UHarmonicsKernel>();
    A->Initialize(1ULL); A->AddMode(2.0, 1.0); A->AddMode(3.0, 1.0); A->AddMode(4.0, 1.0); A->Step(0.01f);
    UHarmonicsKernel* B = NewObject<UHarmonicsKernel>();
    B->Initialize(2ULL); B->AddMode(2.0, 1.0); B->AddMode(3.0, 1.0); B->AddMode(4.0, 1.0); B->Step(0.01f);

    FRealmQuery Q(TEXT("HarmonicRatio"));
    TArray<FRealmQueryResult> RA;
    Cast<IRealmKernel>(A)->QueryAll(Q, RA);
    UTEST_GREATER("QueryAll exposes multiple ratios (not just the strongest)", RA.Num(), 1);

    UCorrespondenceSystem* C = NewObject<UCorrespondenceSystem>();
    C->RegisterRealm(TEXT("A"), TEXT("HarmonicRatio"), A);
    C->RegisterRealm(TEXT("B"), TEXT("HarmonicRatio"), B);
    const int32 Found = C->DetectSharedStructureCorrespondences();
    UTEST_GREATER("Two multi-ratio realms correspond on more than one shared ratio", Found, 1);
    // A realm must not correspond with itself.
    UTEST_EQUAL("Idempotent on re-detect", C->DetectSharedStructureCorrespondences(), 0);

    return true;
}

// Procedural variation: each seed hides a DIFFERENT ratio across all realms (so the
// game can't be pre-solved), and the same seed always hides the same ratio (so runs
// are reproducible). After play, every ratio realm actually realizes that hidden ratio.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralVariationTest, "MANIFOLD.Play.ProceduralVariation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FProceduralVariationTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* A = NewObject<UManifoldSlice>();
    A->Setup(1ULL, 100ULL);
    UManifoldSlice* B = NewObject<UManifoldSlice>();
    B->Setup(2ULL, 100ULL);
    UManifoldSlice* A2 = NewObject<UManifoldSlice>();
    A2->Setup(1ULL, 999ULL); // different fluids seed, same orbits seed

    UTEST_NOT_EQUAL("Different seeds hide different ratios", A->GetSharedRatio(), B->GetSharedRatio());
    UTEST_EQUAL("Same orbits seed hides the same ratio", A->GetSharedRatio(), A2->GetSharedRatio());

    // After play, the realms actually realize the hidden ratio.
    A->RunPlaythrough(30);
    UTEST_EQUAL("Orbits realizes the hidden ratio", A->GetOrbitsRatio(), A->GetSharedRatio());
    UTEST_EQUAL("Harmonics realizes the hidden ratio", A->GetHarmonicsRatio(), A->GetSharedRatio());
    UTEST_EQUAL("Rhythm realizes the hidden ratio", A->GetRhythmRatio(), A->GetSharedRatio());
    UTEST_EQUAL("Circuits realizes the hidden ratio", A->GetCircuitsRatio(), A->GetSharedRatio());
    UTEST_GREATER("The hidden ratio is discovered across domains", A->GetSharedDiscoveries(), 0);
    UTEST_GREATER("The seam ignites on the hidden ratio", A->GetCorrespondencesIgnited(), 0);

    return true;
}

// Expedition (campaign): escalating-difficulty levels played back to back, ending at
// the first level the player can't clear. A session can surface up to 16 discoveries
// (1 seam + C(6,2)=15 cross-domain among the six sharing realms), so a target of 18 is
// the natural difficulty wall.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionTest, "MANIFOLD.Play.Expedition", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FExpeditionTest::RunTest(const FString& Parameters)
{
    // Levels 0..7 demand 2, 4, 6, 8, 10, 12, 14, 16 discoveries — all clearable (<= 16).
    const FManifoldExpeditionResult Eight = UManifoldSlice::RunExpedition(500LL, 8, 30);
    UTEST_EQUAL("Cleared all eight levels", Eight.LevelsCleared, 8);
    UTEST_TRUE("Eight-level expedition completed", Eight.bCompleted);
    UTEST_GREATER("Accumulated a positive total score", Eight.TotalScore, 0);

    // Level 8 demands 18 (> 16), which is unclearable, so the run ends after 8 cleared.
    const FManifoldExpeditionResult Nine = UManifoldSlice::RunExpedition(500LL, 9, 30);
    UTEST_EQUAL("Difficulty wall stops the run at 8 cleared", Nine.LevelsCleared, 8);
    UTEST_FALSE("Nine-level expedition is not completed", Nine.bCompleted);

    // Deterministic in the base seed.
    const FManifoldExpeditionResult Again = UManifoldSlice::RunExpedition(500LL, 8, 30);
    UTEST_EQUAL("Same base seed -> same total score", Again.TotalScore, Eight.TotalScore);

    return true;
}

// Capture replay: an INTERACTIVELY-played session (the player fires the transport
// verb) can be captured as a replay and reproduced bit-for-bit on a fresh slice.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCaptureReplayTest, "MANIFOLD.Play.CaptureReplay", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCaptureReplayTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* S = NewObject<UManifoldSlice>();
    S->Setup(1111ULL, 2222ULL);
    S->bAutoTransportOnIgnite = false; // interactive: the player transports (set after Setup)
    for (int32 i = 0; i < 30; ++i)
    {
        S->Tick();
    }
    UTEST_TRUE("A correspondence is available to the player", S->IsCorrespondenceAvailable());
    UTEST_TRUE("Player transports it", S->PlayerRequestTransport());

    const FManifoldReplay Cap = S->CaptureReplay();
    UTEST_GREATER("Captured a transport", Cap.FinalTransports, 0);
    UTEST_EQUAL("Captured the orbits seed", (int64)Cap.OrbitsSeed, (int64)1111);
    UTEST_EQUAL("Captured the step count", Cap.Steps, 30);

    // Reproduce the captured interactive session on a fresh slice.
    UManifoldSlice* P = NewObject<UManifoldSlice>();
    const FManifoldSliceResult R = P->RunReplay(Cap);
    UTEST_EQUAL("Captured replay reproduces transports", R.TransportsCompleted, Cap.FinalTransports);
    UTEST_EQUAL("Captured replay reproduces Insight Rate", R.InsightRate, Cap.FinalInsightRate);

    return true;
}

// Scoring: a played session yields a positive score that reflects what the player
// surfaced, and the summary carries it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScoringTest, "MANIFOLD.Play.Scoring", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FScoringTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* S = NewObject<UManifoldSlice>();
    S->Setup(1111ULL, 2222ULL);
    S->RunPlaythrough(30);

    const int32 Score = S->GetScore();
    const FManifoldSessionSummary Sum = S->GetSessionSummary();
    UTEST_GREATER("Score is positive after a session", Score, 0);
    UTEST_EQUAL("Summary carries the score", Sum.Score, Score);
    UTEST_GREATER_EQUAL("Score reflects discoveries (1000 each)", Score, Sum.Discoveries * 1000);

    return true;
}

// Rank tiers: the score maps to a grade (D..S), the summary carries it, and a full
// session earns a real grade.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRankTest, "MANIFOLD.Play.Rank", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRankTest::RunTest(const FString& Parameters)
{
    UTEST_EQUAL("D below 3000", (int32)UManifoldSlice::RankForScore(0), (int32)EManifoldRank::D);
    UTEST_EQUAL("C at 3000", (int32)UManifoldSlice::RankForScore(3000), (int32)EManifoldRank::C);
    UTEST_EQUAL("B at 5000", (int32)UManifoldSlice::RankForScore(5000), (int32)EManifoldRank::B);
    UTEST_EQUAL("A at 7000", (int32)UManifoldSlice::RankForScore(7000), (int32)EManifoldRank::A);
    UTEST_EQUAL("S at 9000", (int32)UManifoldSlice::RankForScore(9000), (int32)EManifoldRank::S);
    UTEST_TRUE("Rank is monotonic in score",
        (int32)UManifoldSlice::RankForScore(10000) >= (int32)UManifoldSlice::RankForScore(2000));

    UManifoldSlice* S = NewObject<UManifoldSlice>();
    S->Setup(1111ULL, 2222ULL);
    S->RunPlaythrough(30);
    const FManifoldSessionSummary Sum = S->GetSessionSummary();
    UTEST_TRUE("A full session earns at least a C", (int32)Sum.Rank >= (int32)EManifoldRank::C);
    UTEST_EQUAL("Summary rank matches the score mapping",
        (int32)Sum.Rank, (int32)UManifoldSlice::RankForScore(Sum.Score));

    return true;
}

// Regression (audit): a near-INT32_MAX StepBudget — accepted verbatim by the public SetObjective —
// must NOT overflow the win speed-bonus term (was an int32 "* 10", undefined behavior). The score
// stays non-negative and bounded (the bonus is computed in int64 and capped).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScoreOverflowSafeTest, "MANIFOLD.Play.ScoreOverflowSafe", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FScoreOverflowSafeTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* S = NewObject<UManifoldSlice>();
    FManifoldObjective O;
    O.TargetDiscoveries = 1;          // wins almost immediately, so CurrentStep stays tiny
    O.StepBudget = 2147483647;        // INT32_MAX: old code did (budget - step) * 10 -> int32 overflow
    S->SetObjective(O);
    S->Setup(1111ULL, 2222ULL);
    S->RunPlaythrough(30);

    const int32 Sc = S->GetScore();
    UTEST_TRUE("score won under a huge budget", static_cast<int32>(S->GetSessionState()) == static_cast<int32>(EManifoldSessionState::Won));
    UTEST_TRUE("score is non-negative (no overflow garbage)", Sc >= 0);
    UTEST_TRUE("score is bounded — speed bonus is capped, not overflowed", Sc <= 200000);
    // Determinism: the same objective + seed reproduces the same bounded score.
    UManifoldSlice* S2 = NewObject<UManifoldSlice>();
    S2->SetObjective(O); S2->Setup(1111ULL, 2222ULL); S2->RunPlaythrough(30);
    UTEST_EQUAL("bounded score is deterministic", S2->GetScore(), Sc);
    return true;
}

// Regression (audit): a "best" score must represent a WON run. A high-scoring LOSS (discoveries
// aren't capped at the target; a run can hit the target yet lose on the insight/step gate) must not
// overwrite a lower Won best or report a spurious "new best!".
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLostDoesNotSetBestTest, "MANIFOLD.Integration.LostDoesNotSetBest", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLostDoesNotSetBestTest::RunTest(const FString& Parameters)
{
    FManifoldProfile P;
    FManifoldSessionSummary Won; Won.State = EManifoldSessionState::Won; Won.Score = 4000; Won.bConstellation = false;
    UTEST_TRUE("a won session sets the best", UManifoldSlice::RecordSessionInProfile(P, Won));
    UTEST_EQUAL("classic best is the won score", P.BestScore, 4000);

    FManifoldSessionSummary Lost; Lost.State = EManifoldSessionState::Lost; Lost.Score = 15000; Lost.bConstellation = false;
    UTEST_FALSE("a high-scoring loss is never a new best", UManifoldSlice::RecordSessionInProfile(P, Lost));
    UTEST_EQUAL("a loss does NOT overwrite the won best", P.BestScore, 4000);
    UTEST_EQUAL("played counts the loss", P.SessionsPlayed, 2);
    UTEST_EQUAL("won not incremented by the loss", P.SessionsWon, 1);

    // Same guard on the separate Constellation leaderboard.
    FManifoldSessionSummary CWon; CWon.State = EManifoldSessionState::Won; CWon.Score = 5000; CWon.bConstellation = true;
    UManifoldSlice::RecordSessionInProfile(P, CWon);
    FManifoldSessionSummary CLost; CLost.State = EManifoldSessionState::Lost; CLost.Score = 9000; CLost.bConstellation = true;
    UTEST_FALSE("a constellation loss is not a new best", UManifoldSlice::RecordSessionInProfile(P, CLost));
    UTEST_EQUAL("constellation best unchanged by the loss", P.BestConstellationScore, 5000);
    return true;
}

// Persistent profile: sessions fold into a profile (counts + best score), and the
// profile round-trips through a versioned save file.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfileRoundTripTest, "MANIFOLD.Play.ProfileRoundTrip", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FProfileRoundTripTest::RunTest(const FString& Parameters)
{
    FManifoldProfile P;
    UTEST_EQUAL("Fresh profile best is zero", P.BestScore, 0);

    FManifoldSessionSummary Won;
    Won.State = EManifoldSessionState::Won;
    Won.Score = 500;
    UManifoldSlice::RecordSessionInProfile(P, Won);
    UTEST_EQUAL("Played incremented", P.SessionsPlayed, 1);
    UTEST_EQUAL("Won incremented", P.SessionsWon, 1);
    UTEST_EQUAL("Best updated", P.BestScore, 500);

    FManifoldSessionSummary Lost;
    Lost.State = EManifoldSessionState::Lost;
    Lost.Score = 300; // lower — must not lower the best
    UManifoldSlice::RecordSessionInProfile(P, Lost);
    UTEST_EQUAL("Played incremented again", P.SessionsPlayed, 2);
    UTEST_EQUAL("Won not incremented on a loss", P.SessionsWon, 1);
    UTEST_EQUAL("Best stays at the higher score", P.BestScore, 500);

    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Test.manifoldprofile"));
    UTEST_TRUE("Profile saved", UManifoldSlice::SaveProfile(P, Path));

    FManifoldProfile Loaded;
    UTEST_TRUE("Profile loaded", UManifoldSlice::LoadProfile(Loaded, Path));
    UTEST_EQUAL("Loaded best", Loaded.BestScore, 500);
    UTEST_EQUAL("Loaded played", Loaded.SessionsPlayed, 2);
    UTEST_EQUAL("Loaded won", Loaded.SessionsWon, 1);

    FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);

    // A truncated/corrupt profile (valid header, short body) must FAIL to load AND must
    // NOT partially overwrite the caller's existing profile (which would then be saved
    // back, destroying the real save).
    {
        TArray<uint8> Bytes;
        FMemoryWriter W(Bytes, /*bIsPersistent*/ true);
        uint32 M = FManifoldProfile::Magic;
        uint32 V = FManifoldProfile::Version;
        W << M;
        W << V;
        int32 Partial = 12345;
        W << Partial; // only one of the three body ints — the rest is missing
        const FString BadPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Bad.manifoldprofile"));
        FFileHelper::SaveArrayToFile(Bytes, *BadPath);

        FManifoldProfile Existing;
        Existing.BestScore = 999;
        Existing.SessionsPlayed = 7;
        Existing.SessionsWon = 3;
        const bool bLoaded = UManifoldSlice::LoadProfile(Existing, BadPath);
        UTEST_FALSE("Truncated profile load fails", bLoaded);
        UTEST_EQUAL("Truncated load leaves best score intact", Existing.BestScore, 999);
        UTEST_EQUAL("Truncated load leaves sessions played intact", Existing.SessionsPlayed, 7);
        UTEST_EQUAL("Truncated load leaves sessions won intact", Existing.SessionsWon, 3);
        FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*BadPath);
    }

    return true;
}

// Save-format FORWARD MIGRATION: a returning player's OLDER save (from a build before a format
// bump) must load and carry their career forward — not be silently discarded, which is what the
// old all-or-nothing `Version != current` check did. Fields are append-only, so an old file is a
// valid prefix of the current layout: loading at the file's own version reads its fields and
// defaults the newer ones. A NEWER-than-current file is still rejected (its layout is unknown).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveForwardMigrationTest, "MANIFOLD.Integration.SaveForwardMigration", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSaveForwardMigrationTest::RunTest(const FString& Parameters)
{
    const FString Dir = FPaths::ProjectSavedDir();

    // --- v1 profile (only the original 3 fields) migrates forward, career preserved ---
    {
        TArray<uint8> Bytes;
        FMemoryWriter W(Bytes, /*bIsPersistent*/ true);
        uint32 M = FManifoldProfile::Magic; uint32 V = 1;
        int32 Best = 4200, Played = 9, Won = 4;
        W << M; W << V; W << Best; W << Played; W << Won;
        const FString Path = FPaths::Combine(Dir, TEXT("MigrateV1.manifoldprofile"));
        UTEST_TRUE("write v1 profile", FFileHelper::SaveArrayToFile(Bytes, *Path));

        FManifoldProfile L;
        UTEST_TRUE("v1 profile loads (migrated forward)", UManifoldSlice::LoadProfile(L, Path));
        UTEST_EQUAL("v1 best preserved", L.BestScore, 4200);
        UTEST_EQUAL("v1 played preserved", L.SessionsPlayed, 9);
        UTEST_EQUAL("v1 won preserved", L.SessionsWon, 4);
        UTEST_EQUAL("v1 constellation defaults to 0", L.BestConstellationScore, 0);
        UTEST_EQUAL("v1 expedition defaults to 0", L.BestExpeditionScore, 0);
        FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
    }

    // --- v2 profile (adds constellation, pre-expedition) migrates forward ---
    {
        TArray<uint8> Bytes;
        FMemoryWriter W(Bytes, /*bIsPersistent*/ true);
        uint32 M = FManifoldProfile::Magic; uint32 V = 2;
        int32 Best = 100, Played = 3, Won = 2, Con = 777;
        W << M; W << V; W << Best; W << Played; W << Won; W << Con;
        const FString Path = FPaths::Combine(Dir, TEXT("MigrateV2.manifoldprofile"));
        UTEST_TRUE("write v2 profile", FFileHelper::SaveArrayToFile(Bytes, *Path));

        FManifoldProfile L;
        UTEST_TRUE("v2 profile loads (migrated forward)", UManifoldSlice::LoadProfile(L, Path));
        UTEST_EQUAL("v2 best preserved", L.BestScore, 100);
        UTEST_EQUAL("v2 constellation preserved", L.BestConstellationScore, 777);
        UTEST_EQUAL("v2 expedition defaults to 0", L.BestExpeditionScore, 0);
        FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
    }

    // --- a NEWER-than-current profile is rejected without clobbering the caller ---
    {
        TArray<uint8> Bytes;
        FMemoryWriter W(Bytes, /*bIsPersistent*/ true);
        uint32 M = FManifoldProfile::Magic; uint32 V = FManifoldProfile::Version + 1;
        int32 A = 1, B = 2, C = 3, D = 4, E = 5, Fut = 6;
        W << M; W << V; W << A; W << B; W << C; W << D; W << E; W << Fut;
        const FString Path = FPaths::Combine(Dir, TEXT("MigrateFuture.manifoldprofile"));
        FFileHelper::SaveArrayToFile(Bytes, *Path);

        FManifoldProfile Existing; Existing.BestScore = 555;
        UTEST_FALSE("future-version profile is rejected", UManifoldSlice::LoadProfile(Existing, Path));
        UTEST_EQUAL("rejected future load leaves profile intact", Existing.BestScore, 555);
        FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
    }

    // --- v1 replay (Classic-only, pre-constellation) migrates forward and plays as Classic ---
    {
        TArray<uint8> Bytes;
        FMemoryWriter W(Bytes, /*bIsPersistent*/ true);
        uint32 M = FManifoldReplay::Magic; uint32 V = 1;
        uint64 OrbitsSeed = 12345ULL, FluidsSeed = 67890ULL;
        int32 Steps = 50;
        TArray<int32> Transports; Transports.Add(10); Transports.Add(20);
        int32 Disc = 3, Trans = 2; float Rate = 0.75f;
        W << M; W << V;
        W << OrbitsSeed; W << FluidsSeed; W << Steps; W << Transports;
        W << Disc; W << Trans; W << Rate;
        const FString Path = FPaths::Combine(Dir, TEXT("MigrateV1.manifoldreplay"));
        UTEST_TRUE("write v1 replay", FFileHelper::SaveArrayToFile(Bytes, *Path));

        FManifoldReplay R;
        UTEST_TRUE("v1 replay loads (migrated forward)", UManifoldSlice::LoadReplay(R, Path));
        UTEST_EQUAL("v1 replay orbits seed preserved", static_cast<int64>(R.OrbitsSeed), static_cast<int64>(12345ULL));
        UTEST_EQUAL("v1 replay steps preserved", R.Steps, 50);
        UTEST_EQUAL("v1 replay transport count preserved", R.TransportSteps.Num(), 2);
        UTEST_EQUAL("v1 replay mode defaults to Classic", static_cast<int32>(R.Mode), 0);
        UTEST_EQUAL("v1 replay constellation size defaults to 0", R.ConstellationSize, 0);
        UTEST_EQUAL("v1 replay lock selection empty", R.LockSelection.Num(), 0);
        FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
    }

    return true;
}

// SECURITY: a shareable replay is UNTRUSTED input (authored by another player). A crafted file
// whose TArray count prefix claims billions of elements must be REJECTED (LoadReplay returns
// false), NOT honored as a multi-gigabyte pre-allocation that fatally OOM-crashes the process.
// UE's default TArray serializer pre-allocates count*sizeof(T) before reading any element, and
// its 16MB cap applies only to net archives — so a non-net FMemoryReader would allocate ~8GB for
// a 32-byte file. This exercises the bounded-read guard (SerializeBoundedInt32Array); WITHOUT it
// this very test would OOM-crash the runner. Covers both TArray fields (TransportSteps and the
// v2-only LockSelection, reached past a benign empty TransportSteps), plus a legit small replay
// to prove the guard doesn't over-reject.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMaliciousReplayRejectedTest, "MANIFOLD.Integration.MaliciousReplayRejected", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMaliciousReplayRejectedTest::RunTest(const FString& Parameters)
{
    const FString Dir = FPaths::ProjectSavedDir();
    const int32 HostileCount = 0x7FFFFFFF; // ~2.1e9 elements => ~8GB if honored

    // --- hostile TransportSteps count (v1 body), no element data follows ---
    {
        TArray<uint8> Bytes;
        FMemoryWriter W(Bytes, /*bIsPersistent*/ true);
        uint32 M = FManifoldReplay::Magic; uint32 V = 1;
        uint64 O = 0, F = 0; int32 Steps = 0; int32 Count = HostileCount;
        W << M; W << V; W << O; W << F; W << Steps; W << Count; // file ends — no elements
        const FString Path = FPaths::Combine(Dir, TEXT("EvilTransport.manifoldreplay"));
        UTEST_TRUE("write hostile transport replay", FFileHelper::SaveArrayToFile(Bytes, *Path));

        FManifoldReplay R;
        UTEST_FALSE("hostile TransportSteps count is rejected (no OOM)", UManifoldSlice::LoadReplay(R, Path));
        FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
    }

    // --- hostile LockSelection count (v2), reached past a benign empty TransportSteps ---
    {
        TArray<uint8> Bytes;
        FMemoryWriter W(Bytes, /*bIsPersistent*/ true);
        uint32 M = FManifoldReplay::Magic; uint32 V = 2;
        uint64 O = 0, F = 0; int32 Steps = 0;
        int32 TransportCount = 0;                 // benign, empty
        int32 Disc = 0, Trans = 0; float Rate = 0.0f;
        uint8 Mode = 1; int32 ConSize = 0;
        int32 LockCount = HostileCount;           // hostile
        W << M; W << V; W << O; W << F; W << Steps; W << TransportCount;
        W << Disc; W << Trans; W << Rate; W << Mode; W << ConSize; W << LockCount; // ends — no elements
        const FString Path = FPaths::Combine(Dir, TEXT("EvilLock.manifoldreplay"));
        UTEST_TRUE("write hostile lock replay", FFileHelper::SaveArrayToFile(Bytes, *Path));

        FManifoldReplay R;
        UTEST_FALSE("hostile LockSelection count is rejected (no OOM)", UManifoldSlice::LoadReplay(R, Path));
        FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
    }

    // --- a legitimate small replay still loads (the guard must not over-reject) ---
    {
        TArray<uint8> Bytes;
        FMemoryWriter W(Bytes, /*bIsPersistent*/ true);
        uint32 M = FManifoldReplay::Magic; uint32 V = 1;
        uint64 O = 11ULL, F = 22ULL; int32 Steps = 5;
        int32 Count = 3; int32 A = 1, B = 2, C = 3;
        int32 Disc = 1, Trans = 1; float Rate = 0.5f;
        W << M; W << V; W << O; W << F; W << Steps; W << Count; W << A; W << B; W << C;
        W << Disc; W << Trans; W << Rate;
        const FString Path = FPaths::Combine(Dir, TEXT("GoodSmall.manifoldreplay"));
        UTEST_TRUE("write good replay", FFileHelper::SaveArrayToFile(Bytes, *Path));

        FManifoldReplay R;
        UTEST_TRUE("legit replay still loads", UManifoldSlice::LoadReplay(R, Path));
        UTEST_EQUAL("legit replay transports preserved", R.TransportSteps.Num(), 3);
        UTEST_EQUAL("legit replay last element preserved",
            R.TransportSteps.IsValidIndex(2) ? R.TransportSteps[2] : -1, 3);
        FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
    }

    return true;
}

// Decoy realm (the moat): a red-herring realm exhibits a DIFFERENT ratio, and the
// correspondence engine must refuse to pair it with the true realms. This proves the
// game can't be trivially solved by "everything matches" — the player must actually
// discriminate the real correspondence from the false one.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecoyRealmTest, "MANIFOLD.Play.DecoyRealm", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDecoyRealmTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* S = NewObject<UManifoldSlice>();
    S->Setup(1111ULL, 2222ULL);
    S->RunPlaythrough(30);

    // The decoy realizes a real ratio, but a DIFFERENT one from the hidden ratio.
    UTEST_TRUE("Decoy exhibits a ratio", S->GetDecoyRatio() != FString(TEXT("-")));
    UTEST_NOT_EQUAL("Decoy ratio differs from the hidden ratio (red herring)",
        S->GetDecoyRatio(), S->GetSharedRatio());

    // The six TRUE ratio realms correspond pairwise (C(6,2) = 15). The decoy shares
    // its ratio with none of them, so the engine adds no false correspondence.
    UTEST_EQUAL("Engine refuses the false correspondence (no decoy inflation)",
        S->GetSharedDiscoveries(), 15);

    return true;
}

// ART / ACCESSIBILITY: the seven realm colors (plus the decoy) must stay mutually distinguishable
// even under color-vision deficiency — the game asks players to tell realms apart by colour, so this
// is a real requirement, not decoration. We simulate deuteranopia / protanopia / tritanopia with the
// Machado et al. (2009) severity-1.0 matrices on linear RGB and assert a minimum pairwise separation
// under every condition. The palette is Okabe-Ito, which is designed to clear this; the previous
// ad-hoc palette had near-identical teal (Waves ~ Circuits) and blue (Orbits ~ Fluids) pairs.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPaletteColorblindSafeTest, "MANIFOLD.Art.PaletteColorblindSafe", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPaletteColorblindSafeTest::RunTest(const FString& Parameters)
{
    TArray<FLinearColor> Colors = ManifoldPalette::RealmColors();
    Colors.Add(ManifoldPalette::Decoy); // the decoy must also be distinguishable from every realm

    auto Apply = [](const float M[9], const FLinearColor& C) -> FLinearColor
    {
        return FLinearColor(
            M[0] * C.R + M[1] * C.G + M[2] * C.B,
            M[3] * C.R + M[4] * C.G + M[5] * C.B,
            M[6] * C.R + M[7] * C.G + M[8] * C.B);
    };
    auto Dist = [](const FLinearColor& A, const FLinearColor& B) -> float
    {
        const float dr = A.R - B.R, dg = A.G - B.G, db = A.B - B.B;
        return FMath::Sqrt(dr * dr + dg * dg + db * db);
    };

    // Machado et al. 2009 CVD simulation matrices (severity 1.0), row-major, linear RGB.
    const float Identity[9] = { 1,0,0, 0,1,0, 0,0,1 };
    const float Protan[9]   = { 0.152286f, 1.052583f, -0.204868f,  0.114503f, 0.786281f, 0.099216f, -0.003882f, -0.048116f, 1.051998f };
    const float Deutan[9]   = { 0.367322f, 0.860646f, -0.227968f,  0.280085f, 0.672501f, 0.047413f, -0.011820f,  0.042940f, 0.968881f };
    const float Tritan[9]   = { 1.255528f, -0.076749f, -0.178779f, -0.078411f, 0.930809f, 0.147602f,  0.004733f,  0.691367f, 0.303900f };
    struct FSim { const TCHAR* Name; const float* M; };
    const FSim Sims[4] = { { TEXT("normal"), Identity }, { TEXT("protanopia"), Protan }, { TEXT("deuteranopia"), Deutan }, { TEXT("tritanopia"), Tritan } };

    // Minimum acceptable separation in linear RGB. Okabe-Ito clears this comfortably under every
    // condition; the old palette's Waves/Circuits teal pair fell well under it. (See logged mins.)
    const float Threshold = 0.05f;
    bool bOk = true;
    for (const FSim& Sim : Sims)
    {
        float MinD = FLT_MAX; int32 Ai = -1, Bi = -1;
        for (int32 i = 0; i < Colors.Num(); ++i)
        {
            for (int32 j = i + 1; j < Colors.Num(); ++j)
            {
                const float D = Dist(Apply(Sim.M, Colors[i]), Apply(Sim.M, Colors[j]));
                if (D < MinD) { MinD = D; Ai = i; Bi = j; }
            }
        }
        UE_LOG(LogTemp, Display, TEXT("[PALETTE] %s: min pairwise distance=%.3f (closest pair: colors %d & %d)"), Sim.Name, MinD, Ai, Bi);
        if (!TestTrue(FString::Printf(TEXT("palette stays distinguishable under %s (min %.3f > %.2f)"), Sim.Name, MinD, Threshold), MinD > Threshold))
        {
            bOk = false;
        }
    }
    return bOk;
}

// ---------------------------------------------------------------------------------------------
// BALANCE SWEEP (data-driven playtest, headless). Drives BOTH modes across a large seed set and
// LOGS the difficulty / scoring / economy distributions (via UE_LOG, so a standalone -stdout run
// captures the numbers), then asserts the fairness + discrimination invariants a shipping build
// must always hold. This is the quantitative half of a playtest: it can't measure "feel", but it
// proves the numbers are fair across seeds, the decoy never false-matches, and every constellation
// is solvable — the objective floor beneath a human feel pass.
// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceSweepTest, "MANIFOLD.Balance.Sweep", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBalanceSweepTest::RunTest(const FString& Parameters)
{
    const int32 N = 128;

    // ===== Classic mode: play each seed to its discovery ceiling, measure fairness + rank =====
    int32 MinD = INT32_MAX, MaxD = 0; int64 SumD = 0;
    int32 MinScore = INT32_MAX, MaxScore = 0; int64 SumScore = 0;
    int32 FullRealization = 0;         // seeds reaching SharedDiscoveries == 15 (all C(6,2) pairs)
    int32 DecoyCollisions = 0;         // seeds where decoy ratio == hidden ratio (MUST be 0)
    int32 Degenerate = 0;              // seeds surfacing 0 discoveries (MUST be 0)
    int32 NotDiscDominated = 0;        // seeds whose score isn't discovery-dominated (MUST be 0)
    int32 MaxTransports = 0; float MaxInsight = 0.0f; // component-breakdown diagnostics
    int32 RankHist[5] = { 0, 0, 0, 0, 0 }; // index by EManifoldRank order: D, C, B, A, S

    for (int32 s = 0; s < N; ++s)
    {
        UManifoldSlice* Slice = NewObject<UManifoldSlice>();
        FManifoldObjective Obj; Obj.TargetDiscoveries = 999; Obj.StepBudget = 0; // play to the ceiling
        Slice->SetObjective(Obj);
        Slice->Setup(static_cast<uint64>(s + 1), static_cast<uint64>(s * 7 + 13));
        Slice->RunPlaythrough(60);

        const int32 D = Slice->GetTotalDiscoveries();
        const int32 Shared = Slice->GetSharedDiscoveries();
        const int32 Sc = Slice->GetScore();
        const EManifoldRank R = UManifoldSlice::RankForScore(Sc);

        MinD = FMath::Min(MinD, D); MaxD = FMath::Max(MaxD, D); SumD += D;
        MinScore = FMath::Min(MinScore, Sc); MaxScore = FMath::Max(MaxScore, Sc); SumScore += Sc;
        RankHist[static_cast<int32>(R)]++;
        if (Shared >= 15) { FullRealization++; }
        if (D <= 0) { Degenerate++; }
        if (Slice->GetDecoyRatio() == Slice->GetSharedRatio()) { DecoyCollisions++; }
        MaxTransports = FMath::Max(MaxTransports, Slice->GetTransportsCompleted());
        MaxInsight = FMath::Max(MaxInsight, Slice->GetInsightRate());
        // Score must stay DISCOVERY-dominated: with the transport contribution capped at the
        // discovery count, Score <= D*1000 + D*250 + (small insight term). A generous +2000
        // margin covers insight yet still catches the old unbounded-transport regression (which
        // scored ~116k at D=16, i.e. > D*1250 by ~90k).
        if (Sc > D * 1250 + 2000) { NotDiscDominated++; }
    }

    UE_LOG(LogTemp, Display,
        TEXT("[BALANCE][Classic] N=%d | discoveries min=%d max=%d mean=%.2f | score min=%d max=%d mean=%.0f | fullRealization=%d/%d | ranks D=%d C=%d B=%d A=%d S=%d | decoyCollisions=%d degenerate=%d"),
        N, MinD, MaxD, static_cast<double>(SumD) / N, MinScore, MaxScore, static_cast<double>(SumScore) / N,
        FullRealization, N, RankHist[0], RankHist[1], RankHist[2], RankHist[3], RankHist[4],
        DecoyCollisions, Degenerate);
    UE_LOG(LogTemp, Display, TEXT("[BALANCE][Classic] notDiscDominated=%d/%d (must be 0) | maxTransports=%d maxInsightRate=%.1f (component diagnostics)"),
        NotDiscDominated, N, MaxTransports, MaxInsight);

    // ===== Constellation mode: solvability, relation balance, flawless-solve score =====
    int32 Solvable = 0;
    TMap<FString, int32> RelationHist;
    int32 CMinScore = INT32_MAX, CMaxScore = 0; int64 CSumScore = 0;
    int32 CRankHist[5] = { 0, 0, 0, 0, 0 };

    for (int32 s = 0; s < N; ++s)
    {
        UManifoldSlice* Slice = NewObject<UManifoldSlice>();
        Slice->SetupConstellation(static_cast<int64>(s + 1), 3, /*bExpert*/ false);
        RelationHist.FindOrAdd(Slice->GetActiveRelationName())++;
        const TArray<int32> Answer = Slice->GetConstellation();
        if (Slice->PlayerLockConstellation(Answer)) { Solvable++; }
        const int32 Sc = Slice->GetScore();
        CMinScore = FMath::Min(CMinScore, Sc); CMaxScore = FMath::Max(CMaxScore, Sc); CSumScore += Sc;
        CRankHist[static_cast<int32>(UManifoldSlice::RankForScore(Sc))]++;
    }

    FString RelStr;
    for (const TPair<FString, int32>& P : RelationHist) { RelStr += FString::Printf(TEXT("%s=%d "), *P.Key, P.Value); }
    UE_LOG(LogTemp, Display,
        TEXT("[BALANCE][Constellation] N=%d | solvable=%d/%d | relations: %s| score min=%d max=%d mean=%.0f | ranks D=%d C=%d B=%d A=%d S=%d"),
        N, Solvable, N, *RelStr, CMinScore, CMaxScore, static_cast<double>(CSumScore) / N,
        CRankHist[0], CRankHist[1], CRankHist[2], CRankHist[3], CRankHist[4]);

    // ===== Invariants a shipping build must ALWAYS hold (objective, not feel) =====
    UTEST_EQUAL("Classic: decoy never collides with the hidden ratio", DecoyCollisions, 0);
    UTEST_EQUAL("Classic: no degenerate (zero-discovery) seed", Degenerate, 0);
    UTEST_EQUAL("Classic: score stays discovery-dominated (transport term capped)", NotDiscDominated, 0);
    UTEST_EQUAL("Constellation: every seed is solvable by the correct lock", Solvable, N);
    UTEST_TRUE("Constellation: both relations occur (rule inference isn't degenerate to one)", RelationHist.Num() >= 2);

    return true;
}

// Stable structure identity: a realm's Query must return the SAME id for the same
// detected structure across calls (not a fresh GUID each time), so the correspondence
// layer can dedup and transport by it. Regression guard for the Harmonics/Waves/Rhythm
// kernels, which previously returned FGuid::NewGuid() on every query.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStableStructureIdTest, "MANIFOLD.Play.StableStructureIds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStableStructureIdTest::RunTest(const FString& Parameters)
{
    UHarmonicsKernel* H = NewObject<UHarmonicsKernel>();
    H->Initialize(1ULL); H->AddMode(2.0, 1.0); H->AddMode(3.0, 1.0); H->Step(0.01f);

    UWavesKernel* W = NewObject<UWavesKernel>();
    W->Initialize(2ULL); W->ExciteHarmonic(2, 1.0); W->ExciteHarmonic(3, 1.0); W->Step(0.001f);

    URhythmKernel* R = NewObject<URhythmKernel>();
    R->Initialize(3ULL); R->AddVoice(3.0); R->AddVoice(2.0); R->Step(0.016f);

    struct FCase { IRealmKernel* Kernel; FName Query; const TCHAR* Name; };
    const FCase Cases[] = {
        { Cast<IRealmKernel>(H), TEXT("HarmonicRatio"), TEXT("Harmonics") },
        { Cast<IRealmKernel>(W), TEXT("WaveHarmonic"),  TEXT("Waves") },
        { Cast<IRealmKernel>(R), TEXT("RhythmRatio"),   TEXT("Rhythm") },
    };

    for (const FCase& C : Cases)
    {
        FRealmQuery Q(C.Query);
        FRealmQueryResult R1, R2;
        const bool bOk1 = C.Kernel->Query(Q, R1);
        const bool bOk2 = C.Kernel->Query(Q, R2);
        UTEST_TRUE(FString::Printf(TEXT("%s query succeeds"), C.Name), bOk1 && bOk2);
        UTEST_TRUE(FString::Printf(TEXT("%s id is valid (not zero)"), C.Name), R1.StructureId.IsValid());
        UTEST_EQUAL(FString::Printf(TEXT("%s id is stable across calls"), C.Name), R1.StructureId, R2.StructureId);
    }

    // Different realms with the same ratio must still have DISTINCT ids.
    FRealmQueryResult HR, WR;
    Cast<IRealmKernel>(H)->Query(FRealmQuery(TEXT("HarmonicRatio")), HR);
    Cast<IRealmKernel>(W)->Query(FRealmQuery(TEXT("WaveHarmonic")), WR);
    UTEST_NOT_EQUAL("Distinct realms have distinct ids", HR.StructureId, WR.StructureId);

    return true;
}

// Audio integration: playing the slice emits exactly one audio cue per discovery
// (a chime voicing its ratio) and one per transport (a chord-resolve). This is the
// event routing the audio pipeline binds sounds to.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSliceAudioIntegrationTest, "MANIFOLD.Play.AudioIntegration", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSliceAudioIntegrationTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    Slice->Setup(1111ULL, 2222ULL);
    Slice->RunPlaythrough(30);

    UTEST_GREATER("Audio cues were emitted during play", Slice->GetAudioCueCount(), 0);
    // One cue per seam ignition, per cross-domain discovery, and per transport.
    UTEST_EQUAL("One cue per ignition + discovery + transport",
        Slice->GetAudioCueCount(),
        Slice->GetCorrespondencesIgnited() + Slice->GetSharedDiscoveries() + Slice->GetTransportsCompleted());
    UTEST_TRUE("Last cue carries a realm identity", Slice->GetLastAudioCue().Realm != NAME_None);

    return true;
}
