// The Last Watch

#include "KeeperSynth.h"

namespace
{
	constexpr float TwoPi = 2.0f * PI;

	/** Per-sample smoothing coefficient for a time constant of Seconds */
	float SmoothCoefficient(float Seconds, float Rate)
	{
		return 1.0f - FMath::Exp(-1.0f / (Seconds * Rate));
	}
}

UKeeperSynth::UKeeperSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 2;
	bAllowSpatialization = false;
	bAutoActivate = true;
}

bool UKeeperSynth::Init(int32& SampleRate)
{
	NumChannels = 2;
	Rate = static_cast<float>(SampleRate);
	return true;
}

void UKeeperSynth::SetRadio(float Static, float Voice, float Detune)
{
	RadioStatic.store(Static);
	RadioVoice.store(Voice);
	RadioDetune.store(Detune);
}

float UKeeperSynth::Noise()
{
	// xorshift32
	Seed ^= Seed << 13;
	Seed ^= Seed >> 17;
	Seed ^= Seed << 5;
	return static_cast<float>(Seed) / 2147483648.0f - 1.0f;
}

float UKeeperSynth::Smooth(float Current, float Target, float Coefficient)
{
	return Current + (Target - Current) * Coefficient;
}

float UKeeperSynth::BandPass(FBand& Filter, float Input, float Frequency, float Q) const
{
	// Chamberlin state-variable filter
	const float F = 2.0f * FMath::Sin(PI * FMath::Min(Frequency, Rate * 0.2f) / Rate);
	const float High = Input - Filter.Low - Filter.Band / Q;
	Filter.Band += F * High;
	Filter.Low += F * Filter.Band;
	return Filter.Band;
}

int32 UKeeperSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const float Dt = 1.0f / Rate;
	const float Slow = SmoothCoefficient(0.6f, Rate);
	const float Fast = SmoothCoefficient(0.08f, Rate);

	const float DreadTarget = Dread.load();
	const float PanicTarget = Panic.load();
	const float RainTarget = Rain.load();
	const float MuffleTarget = RainMuffle.load();
	const float WhisperTarget = Whisper.load();
	const float StaticTarget = RadioStatic.load();
	const float VoiceTarget = RadioVoice.load();
	const float DetuneTarget = RadioDetune.load();

	if (const float Strength = PendingSting.exchange(0.0f); Strength > 0.0f)
	{
		StingAge = 0.0f;
		StingEnv = Strength;
	}
	if (PendingKnock.exchange(false))
	{
		KnockAge = 0.0f;
	}
	if (PendingClick.exchange(false))
	{
		ClickAge = 0.0f;
	}

	// one-pole low-pass coefficients
	const float RumbleA = TwoPi * 70.0f / Rate;

	for (int32 Frame = 0; Frame + 1 < NumSamples; Frame += 2)
	{
		DreadLevel = Smooth(DreadLevel, DreadTarget, Slow);
		PanicLevel = Smooth(PanicLevel, PanicTarget, Slow);
		RainLevel = Smooth(RainLevel, RainTarget, Slow);
		MuffleLevel = Smooth(MuffleLevel, MuffleTarget, Slow);
		WhisperLevel = Smooth(WhisperLevel, WhisperTarget, Slow);
		StaticLevel = Smooth(StaticLevel, StaticTarget, Fast);
		VoiceLevel = Smooth(VoiceLevel, VoiceTarget, Fast);
		DetuneLevel = Smooth(DetuneLevel, DetuneTarget, Fast);
		Time += Dt;
		const float T = static_cast<float>(Time);

		float Mono = 0.0f;
		float Left = 0.0f;
		float Right = 0.0f;

		// --- drone: two beating low sines, a fifth above, and a rumble of filtered noise
		if (DreadLevel > 0.001f)
		{
			const float Freqs[3] = { 41.2f, 41.65f, 61.8f };
			float Drone = 0.0f;
			for (int32 i = 0; i < 3; ++i)
			{
				DronePhase[i] = FMath::Fmod(DronePhase[i] + Freqs[i] * Dt, 1.0);
				const float P = static_cast<float>(DronePhase[i]) * TwoPi;
				// the second harmonic carries it on small speakers
				Drone += FMath::Sin(P) + 0.45f * FMath::Sin(2.0f * P);
			}
			RumbleLP += RumbleA * (Noise() - RumbleLP);
			const float Swell = 0.75f + 0.25f * FMath::Sin(T * 0.21f) * FMath::Sin(T * 0.077f);
			Mono += (Drone * 0.07f + RumbleLP * 2.2f) * DreadLevel * Swell;
		}

		// --- heartbeat: lub-dub, faster with panic
		if (PanicLevel > 0.002f)
		{
			const double Period = 60.0 / (55.0 + 65.0 * PanicLevel);
			const double Before = HeartClock;
			HeartClock += Dt;
			if (HeartClock >= Period)
			{
				HeartClock -= Period;
				HeartEnv = 1.0f;
				HeartPhase = 0.0;
			}
			else if (Before < Period * 0.3 && HeartClock >= Period * 0.3)
			{
				HeartEnv = 0.7f;
				HeartPhase = 0.0;
			}
			HeartPhase += Dt;
			const float Body = FMath::Sin(TwoPi * 52.0f * static_cast<float>(HeartPhase)) + 0.5f * FMath::Sin(TwoPi * 104.0f * static_cast<float>(HeartPhase));
			HeartEnv *= 1.0f - Dt / 0.085f;
			Mono += Body * HeartEnv * PanicLevel * 0.5f;
		}

		// --- rain: band-limited hiss with the odd heavier drop; muffled indoors
		if (RainLevel > 0.001f)
		{
			const float In = Noise();
			RainHP = 0.985f * (RainHP + In - RainHPIn);
			RainHPIn = In;
			const float Cutoff = FMath::Lerp(7000.0f, 500.0f, MuffleLevel);
			RainLP += (TwoPi * Cutoff / Rate) * (RainHP - RainLP);
			if (Noise() > 1.0f - 0.0006f * RainLevel)
			{
				RainDrop = Noise() * 0.8f;
			}
			RainDrop *= 0.995f;
			const float Drops = RainDrop * (1.0f - MuffleLevel * 0.7f);
			Mono += (RainLP * 0.32f + Drops * 0.25f) * RainLevel;
		}

		// --- whisper: noise through two slowly wandering formants, breathing in and out, panned around
		if (WhisperLevel > 0.001f)
		{
			const float In = Noise();
			const float F1 = 650.0f + 350.0f * FMath::Sin(T * 0.53f) + 120.0f * FMath::Sin(T * 2.3f);
			const float F2 = 1900.0f + 700.0f * FMath::Sin(T * 0.31f + 1.0f);
			const float Voice = BandPass(WhisperBands[0], In, F1, 9.0f) + 0.6f * BandPass(WhisperBands[1], In, F2, 12.0f);
			const float Breath = FMath::Max(0.0f, FMath::Sin(T * 1.7f) * FMath::Sin(T * 0.43f + 0.5f));
			WhisperMod = Smooth(WhisperMod, Breath, 0.0005f);
			const float Pan = 0.5f + 0.45f * FMath::Sin(T * 0.37f);
			const float Out = Voice * WhisperMod * WhisperLevel * 0.9f;
			Left += Out * (1.0f - Pan);
			Right += Out * Pan;
		}

		// --- radio: hiss and crackle, a voice-like murmur, a heterodyne whistle off-station
		if (StaticLevel > 0.001f || VoiceLevel > 0.001f)
		{
			if (Noise() > 0.9996f)
			{
				StaticCrackle = Noise();
			}
			StaticCrackle *= 0.97f;
			Mono += (Noise() * 0.14f + StaticCrackle * 0.35f) * StaticLevel;

			if (VoiceLevel > 0.001f)
			{
				SyllableClock -= Dt;
				if (SyllableClock <= 0.0)
				{
					// next syllable: new length, new vowel; now and then a pause between words
					SyllableClock = 0.11 + 0.17 * (Noise() * 0.5 + 0.5);
					SyllableFormant = 450.0f + 650.0f * (Noise() * 0.5f + 0.5f);
					SyllableEnv = Noise() > -0.6f ? 1.0f : 0.0f;
				}
				WhistlePhase = FMath::Fmod(WhistlePhase + (118.0 + 14.0 * FMath::Sin(T * 3.1f)) * Dt, 1.0);
				const float Buzz = static_cast<float>(WhistlePhase) * 2.0f - 1.0f;
				const float Source = Buzz * 0.6f + Noise() * 0.4f;
				const float Speech = BandPass(VoiceBands[0], Source, SyllableFormant, 6.0f)
					+ 0.5f * BandPass(VoiceBands[1], Source, SyllableFormant * 2.6f, 8.0f);
				VoiceEnv = Smooth(VoiceEnv, SyllableEnv * static_cast<float>(FMath::Min(1.0, SyllableClock * 12.0)), 0.004f);
				Mono += Speech * VoiceEnv * VoiceLevel * 0.55f;
			}

			if (DetuneLevel > 0.01f && DetuneLevel < 0.99f)
			{
				const float Whistle = FMath::Sin(T * TwoPi * (250.0f + DetuneLevel * 2200.0f));
				Mono += Whistle * 0.05f * 4.0f * DetuneLevel * (1.0f - DetuneLevel) * StaticLevel;
			}
		}

		// --- sting: a cluster that swells and slides down
		if (StingEnv > 0.0f && StingAge < 7.0f)
		{
			StingAge += Dt;
			const float Env = StingAge < 0.35f ? StingAge / 0.35f : FMath::Exp(-(StingAge - 0.35f) / 1.6f);
			const float Glide = 1.0f - 0.06f * FMath::Min(StingAge, 5.0f);
			const float Freqs[5] = { 110.0f, 116.5f, 155.6f, 233.1f, 329.6f };
			float Cluster = 0.0f;
			for (int32 i = 0; i < 5; ++i)
			{
				StingPhase[i] = FMath::Fmod(StingPhase[i] + Freqs[i] * Glide * Dt, 1.0);
				const float P = static_cast<float>(StingPhase[i]);
				// soft saw for bite
				Cluster += (P * 2.0f - 1.0f) * 0.35f + FMath::Sin(P * TwoPi) * 0.65f;
			}
			Mono += (Cluster * 0.07f + Noise() * 0.05f * Env) * Env * StingEnv;
		}

		// --- knocks: three thumps on a door
		if (KnockAge < 1.2f)
		{
			KnockAge += Dt;
			for (const float At : { 0.0f, 0.36f, 0.68f })
			{
				const float K = KnockAge - At;
				if (K >= 0.0f && K < 0.25f)
				{
					KnockLP += (TwoPi * 900.0f / Rate) * (Noise() - KnockLP);
					Mono += (FMath::Sin(TwoPi * 82.0f * K) * FMath::Exp(-K / 0.06f) * 0.8f + KnockLP * FMath::Exp(-K / 0.015f) * 2.0f) * 0.7f;
				}
			}
		}

		// --- click
		if (ClickAge < 0.03f)
		{
			ClickAge += Dt;
			Mono += (Noise() * FMath::Exp(-ClickAge / 0.002f) + FMath::Sin(TwoPi * 2400.0f * ClickAge) * FMath::Exp(-ClickAge / 0.008f)) * 0.25f;
		}

		// soft clip so stacked layers never crack
		OutAudio[Frame] = FMath::Clamp(FMath::Tanh(Mono + Left), -1.0f, 1.0f);
		OutAudio[Frame + 1] = FMath::Clamp(FMath::Tanh(Mono + Right), -1.0f, 1.0f);
	}

	// keep the clock bounded so float sine arguments keep their precision
	if (Time > 3600.0)
	{
		Time -= 3600.0;
	}
	return NumSamples;
}
