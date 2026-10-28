// The Last Watch

#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "KeeperSynth.generated.h"

/**
 *  Every sound the game makes up on the spot rather than loading from an asset:
 *  the dread drone, a heartbeat, rain, radio static and a voice behind it, whispering,
 *  and one-shot stings, knocks and clicks.
 *
 *  Layers are mixed on the audio thread; the game thread only moves their target levels
 *  (0..1), which are smoothed per sample so nothing clicks.
 */
UCLASS(ClassGroup=(LastWatch))
class THELASTWATCH_API UKeeperSynth : public USynthComponent
{
	GENERATED_BODY()

public:

	UKeeperSynth(const FObjectInitializer& ObjectInitializer);

	/** Low drone and rumble under everything */
	void SetDread(float Level) { Dread.store(Level); }

	/** Heartbeat; its rate rises with the level too */
	void SetPanic(float Level) { Panic.store(Level); }

	void SetRain(float Level) { Rain.store(Level); }

	/** Muffles the rain (indoors) */
	void SetRainMuffle(float Amount) { RainMuffle.store(Amount); }

	/** Breathy choir of filtered noise */
	void SetWhisper(float Level) { Whisper.store(Level); }

	/** Radio: hiss, a murmuring voice under it, and a whistle that rises as the dial moves off a station */
	void SetRadio(float Static, float Voice, float Detune);

	/** Dissonant swell that dies away over a few seconds */
	void Sting(float Strength = 1.0f) { PendingSting.store(Strength); }

	/** Three heavy knocks on wood */
	void Knock() { PendingKnock.store(true); }

	/** Small mechanical click (switches) */
	void Click() { PendingClick.store(true); }

	/** The keeper's repeater watch striking: Strikes small high tings */
	void Chime(int32 Strikes) { PendingChime.store(Strikes); }

	/** A deep bell somewhere out in the fog, tolling Strikes times */
	void Toll(int32 Strikes) { PendingToll.store(Strikes); }

	/** Ratchet of the clockwork being wound; 0 = silent, 1 = winding hard */
	void SetRatchet(float Level) { Ratchet.store(Level); }

protected:

	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:

	// targets from the game thread
	std::atomic<float> Dread{0.0f};
	std::atomic<float> Panic{0.0f};
	std::atomic<float> Rain{0.0f};
	std::atomic<float> RainMuffle{0.0f};
	std::atomic<float> Whisper{0.0f};
	std::atomic<float> RadioStatic{0.0f};
	std::atomic<float> RadioVoice{0.0f};
	std::atomic<float> RadioDetune{0.0f};
	std::atomic<float> PendingSting{0.0f};
	std::atomic<bool> PendingKnock{false};
	std::atomic<bool> PendingClick{false};
	std::atomic<int32> PendingChime{0};
	std::atomic<int32> PendingToll{0};
	std::atomic<float> Ratchet{0.0f};

	// audio thread state
	float Rate = 48000.0f;
	uint32 Seed = 0x1974u;
	double Time = 0.0;

	float DreadLevel = 0.0f, PanicLevel = 0.0f, RainLevel = 0.0f, MuffleLevel = 0.0f, WhisperLevel = 0.0f;
	float StaticLevel = 0.0f, VoiceLevel = 0.0f, DetuneLevel = 0.0f;

	double DronePhase[3] = {};
	float RumbleLP = 0.0f;

	double HeartClock = 0.0;
	float HeartEnv = 0.0f;
	double HeartPhase = 0.0;

	float RainLP = 0.0f, RainHP = 0.0f, RainHPIn = 0.0f, RainDrop = 0.0f;
	float MuffleLP = 0.0f;

	/** State-variable band-pass filters: whisper formants (2) and the radio voice (2) */
	struct FBand { float Low = 0.0f; float Band = 0.0f; };
	FBand WhisperBands[2];
	FBand VoiceBands[2];
	float WhisperMod = 0.0f;
	float SyllableEnv = 0.0f;
	float VoiceEnv = 0.0f;
	double SyllableClock = 0.0;
	float SyllableFormant = 700.0f;
	double WhistlePhase = 0.0;
	float StaticCrackle = 0.0f;

	float StingEnv = 0.0f;
	float StingAge = 0.0f;
	double StingPhase[5] = {};

	float KnockAge = 10.0f;
	float KnockLP = 0.0f;

	float ClickAge = 10.0f;

	/** Struck bell: a few inharmonic partials decaying at their own rates, struck Strikes times */
	struct FBell
	{
		int32 StrikesLeft = 0;
		float UntilNext = 0.0f;
		float Age = 100.0f;
		double Phase[5] = {};
	};
	FBell ChimeBell;
	FBell TollBell;
	float TollLP = 0.0f;

	double RatchetClock = 0.0;
	float RatchetAge = 10.0f;
	float RatchetLevel = 0.0f;

	/** Advances a bell by one sample and returns its output */
	float RingBell(FBell& Bell, float Dt, float Base, const float* Ratios, const float* Decays, const float* Gains, float Interval);

	float Noise();
	static float Smooth(float Current, float Target, float Coefficient);
	float BandPass(FBand& Filter, float Input, float Frequency, float Q) const;
};
