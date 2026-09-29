#include "session_engine.hpp"
#include "test_utils.hpp"
#include "text_utils.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace typing_trainer
{
namespace
{

using test::Clock;
using test::control;
using test::events_of;
using test::key;
using test::milliseconds;
using test::start_free;

/// \brief Последнее событие-обновление из пачки (ожидается, что оно есть).
StateUpdate last_update(const std::vector<BackendEvent>& events)
{
	auto const updates = events_of<StateUpdate>(events);
	EXPECT_FALSE(updates.empty());
	return updates.empty() ? StateUpdate{} : updates.back();
}

class SessionEngineTest : public ::testing::Test
{
protected:
	/// \brief Напечатать строку, начиная с момента now_, с шагом step.
	std::vector<BackendEvent> type(const std::u32string& text,
	                               milliseconds          step = milliseconds(200))
	{
		std::vector<BackendEvent> all;
		for (char32_t const ch : text)
		{
			now_ += step;
			auto events = engine_.handle(key(ch, now_));
			all.insert(all.end(), events.begin(), events.end());
		}
		return all;
	}

	/// \brief Нажать Enter через step после предыдущего нажатия.
	std::vector<BackendEvent> enter(milliseconds step = milliseconds(200))
	{
		now_ += step;
		return engine_.handle(control(ControlKey::Enter, now_));
	}

	SessionEngine     engine_;
	Clock::time_point now_ = Clock::now();
};

TEST_F(SessionEngineTest, StartFreeSessionPublishesText)
{
	auto const states = events_of<SessionState>(engine_.handle(start_free(U"abc")));

	ASSERT_EQ(states.size(), 1U);
	EXPECT_EQ(states[0].status, SessionStatus::Active);
	EXPECT_EQ(states[0].cursor_position, 0U);
	ASSERT_EQ(states[0].chars.size(), 3U);
	for (auto const& ch : states[0].chars)
		EXPECT_EQ(ch.status, CharStatus::Pending);
}

TEST_F(SessionEngineTest, CorrectAndWrongKeysAreMarked)
{
	(void)engine_.handle(start_free(U"abc"));

	auto const first = last_update(type(U"a"));
	EXPECT_EQ(first.changed_index, 0U);
	EXPECT_EQ(first.changed_char.status, CharStatus::Correct);
	EXPECT_EQ(first.cursor_position, 1U);

	auto const second = last_update(type(U"x"));
	EXPECT_EQ(second.changed_index, 1U);
	EXPECT_EQ(second.changed_char.status, CharStatus::Wrong);
	EXPECT_DOUBLE_EQ(second.metrics.accuracy, 50.0);
}

TEST_F(SessionEngineTest, TypingWholeTextCompletesSession)
{
	(void)engine_.handle(start_free(U"hi"));

	auto const update = last_update(type(U"hi"));
	EXPECT_TRUE(update.is_completed);
	EXPECT_EQ(update.status, SessionStatus::Completed);

	// После завершения ввод игнорируется.
	EXPECT_TRUE(type(U"x").empty());
}

TEST_F(SessionEngineTest, BackspaceReturnsCursorAndResetsChar)
{
	(void)engine_.handle(start_free(U"abc"));
	(void)type(U"ax");

	auto const update = last_update(engine_.handle(control(ControlKey::Backspace)));
	EXPECT_EQ(update.changed_index, 1U);
	EXPECT_EQ(update.changed_char.status, CharStatus::Pending);
	EXPECT_EQ(update.cursor_position, 1U);

	auto const retyped = last_update(type(U"b"));
	EXPECT_EQ(retyped.changed_index, 1U);
	EXPECT_EQ(retyped.changed_char.status, CharStatus::Correct);
}

TEST_F(SessionEngineTest, BackspaceAtStartDoesNothing)
{
	(void)engine_.handle(start_free(U"abc"));
	EXPECT_TRUE(engine_.handle(control(ControlKey::Backspace)).empty());
}

TEST_F(SessionEngineTest, PauseBlocksInputUntilResume)
{
	(void)engine_.handle(start_free(U"abc"));

	auto const paused = events_of<SessionState>(engine_.handle(PauseSessionCommand{}));
	ASSERT_EQ(paused.size(), 1U);
	EXPECT_EQ(paused[0].status, SessionStatus::Paused);
	EXPECT_TRUE(type(U"a").empty());

	auto const resumed = events_of<SessionState>(engine_.handle(ResumeSessionCommand{}));
	ASSERT_EQ(resumed.size(), 1U);
	EXPECT_EQ(resumed[0].status, SessionStatus::Active);
	EXPECT_EQ(last_update(type(U"a")).changed_char.status, CharStatus::Correct);
}

TEST_F(SessionEngineTest, EscapePauses)
{
	(void)engine_.handle(start_free(U"abc"));

	auto const states = events_of<SessionState>(engine_.handle(control(ControlKey::Escape)));
	ASSERT_EQ(states.size(), 1U);
	EXPECT_EQ(states[0].status, SessionStatus::Paused);
}

TEST_F(SessionEngineTest, StopClearsSession)
{
	(void)engine_.handle(start_free(U"abc"));
	(void)type(U"ab");

	auto const states = events_of<SessionState>(engine_.handle(StopSessionCommand{}));
	ASSERT_EQ(states.size(), 1U);
	EXPECT_EQ(states[0].status, SessionStatus::Inactive);
	EXPECT_TRUE(states[0].chars.empty());
	EXPECT_DOUBLE_EQ(states[0].metrics.cpm, 0.0);
}

TEST_F(SessionEngineTest, IgnoreCaseAcceptsOtherCase)
{
	(void)engine_.handle(start_free(U"Ab Ёж", /*ignore_case=*/true));

	for (auto const& update : events_of<StateUpdate>(type(U"aB ёЖ")))
		EXPECT_EQ(update.changed_char.status, CharStatus::Correct);
}

TEST_F(SessionEngineTest, CaseMattersByDefault)
{
	(void)engine_.handle(start_free(U"A"));
	EXPECT_EQ(last_update(type(U"a")).changed_char.status, CharStatus::Wrong);
}

TEST_F(SessionEngineTest, SmartSessionGeneratesTextOfRequestedLength)
{
	SessionConfig config;
	config.mode          = TrainingMode::Smart;
	config.language      = Language::Russian;
	config.target_length = 120;

	auto const states
	    = events_of<SessionState>(engine_.handle(StartSessionCommand{.config = config}));
	ASSERT_EQ(states.size(), 1U);
	EXPECT_GE(states[0].chars.size(), 120U);
}

TEST_F(SessionEngineTest, TimerStartsWithFirstKeystroke)
{
	(void)engine_.handle(start_free(U"abcdefghijk"));

	now_ += std::chrono::seconds(30); // пользователь читал текст - это не время набора
	auto const update = last_update(type(U"abcdefghijk", milliseconds(200)));

	// Первое нажатие через 200 мс после «чтения», дальше 10 интервалов по 200 мс.
	EXPECT_NEAR(update.metrics.elapsed_seconds, 2.0, 1e-9);
	EXPECT_NEAR(update.metrics.cpm, 11.0 / 2.0 * 60.0, 1e-9);
	EXPECT_NEAR(update.metrics.wpm, update.metrics.cpm / 5.0, 1e-9);
	EXPECT_EQ(update.metrics.keystrokes, 11U);
}

TEST_F(SessionEngineTest, IdleTimeBeforePauseIsNotCounted)
{
	(void)engine_.handle(start_free(U"abcdefghij"));
	(void)type(U"abcde", milliseconds(200)); // отрезок 0.8 с

	now_ += std::chrono::seconds(20); // отошёл, сработала автопауза
	(void)engine_.handle(PauseSessionCommand{});
	now_ += std::chrono::seconds(60);
	(void)engine_.handle(ResumeSessionCommand{});

	now_ += std::chrono::seconds(5); // первое нажатие после паузы открывает новый отрезок
	auto const update = last_update(type(U"fghij", milliseconds(200)));

	// Два отрезка по 4 интервала: ни простой перед паузой, ни ожидание после неё не в счёт.
	EXPECT_NEAR(update.metrics.elapsed_seconds, 0.8 + 0.8, 1e-9);
}

TEST_F(SessionEngineTest, SpeedIsHiddenDuringFirstSecond)
{
	(void)engine_.handle(start_free(U"abcdefgh"));

	auto const early = last_update(type(U"abc", milliseconds(100)));
	EXPECT_DOUBLE_EQ(early.metrics.cpm, 0.0);
	EXPECT_DOUBLE_EQ(early.metrics.wpm, 0.0);
}

TEST_F(SessionEngineTest, ShortTextStillGetsFinalSpeed)
{
	(void)engine_.handle(start_free(U"ab"));

	auto const update = last_update(type(U"ab", milliseconds(250)));
	ASSERT_TRUE(update.is_completed);
	EXPECT_NEAR(update.metrics.cpm, 2.0 / 0.25 * 60.0, 1e-9);
}

TEST_F(SessionEngineTest, CorrectedCharsCountForSpeed)
{
	(void)engine_.handle(start_free(U"abcdefgh"));
	(void)type(U"ax", milliseconds(200));
	(void)engine_.handle(control(ControlKey::Backspace, now_));
	auto const update = last_update(type(U"bcdefgh", milliseconds(200)));

	ASSERT_TRUE(update.is_completed);
	// 8 верных символов; ошибка осталась в точности.
	EXPECT_NEAR(update.metrics.cpm, 8.0 / update.metrics.elapsed_seconds * 60.0, 1e-9);
	EXPECT_NEAR(update.metrics.accuracy, 8.0 / 9.0 * 100.0, 1e-9);
}

TEST_F(SessionEngineTest, SteadyRhythmScoresHigherThanUneven)
{
	(void)engine_.handle(start_free(U"abcdefghijklmnop"));
	auto const steady = last_update(type(U"abcdefghijklmnop", milliseconds(200)));

	SessionEngine uneven_engine;
	(void)uneven_engine.handle(start_free(U"abcdefghijklmnop"));
	auto        time = now_;
	StateUpdate uneven;
	int         step = 0;
	for (char32_t const ch : std::u32string(U"abcdefghijklmnop"))
	{
		time += milliseconds(step++ % 2 == 0 ? 80 : 450);
		uneven = last_update(uneven_engine.handle(key(ch, time)));
	}

	EXPECT_NEAR(steady.metrics.consistency, 100.0, 1e-9);
	EXPECT_GT(uneven.metrics.consistency, 0.0);
	EXPECT_LT(uneven.metrics.consistency, steady.metrics.consistency);
}

TEST_F(SessionEngineTest, RhythmNeedsSeveralIntervals)
{
	(void)engine_.handle(start_free(U"abcdefgh"));
	EXPECT_DOUBLE_EQ(last_update(type(U"abc")).metrics.consistency, 0.0);
}

TEST_F(SessionEngineTest, WrongLayoutKeysAreReportedButNotCounted)
{
	(void)engine_.handle(start_free(U"hello"));

	auto const events   = type(U"р"); // та же клавиша, что «h», но в русской раскладке
	auto const warnings = events_of<LayoutMismatch>(events);
	ASSERT_EQ(warnings.size(), 1U);
	EXPECT_EQ(warnings[0].expected, U'h');
	EXPECT_EQ(warnings[0].pressed, U'р');
	EXPECT_TRUE(events_of<StateUpdate>(events).empty());

	auto const update = last_update(type(U"h"));
	EXPECT_EQ(update.changed_index, 0U);
	EXPECT_EQ(update.changed_char.status, CharStatus::Correct);
	EXPECT_DOUBLE_EQ(update.metrics.accuracy, 100.0);
	EXPECT_EQ(update.metrics.keystrokes, 1U);
}

TEST_F(SessionEngineTest, CustomTextIsNormalized)
{
	auto const states
	    = events_of<SessionState>(engine_.handle(start_free(U"  two  words  \n\n\n  lines  ")));
	ASSERT_EQ(states.size(), 1U);

	std::u32string text;
	for (auto const& ch : states[0].chars)
		text.push_back(ch.character);
	EXPECT_EQ(text, U"two words\n\nlines");
}

TEST_F(SessionEngineTest, EnterTypesLineBreakOnlyAtLineEnd)
{
	(void)engine_.handle(start_free(U"ab\ncd"));
	(void)type(U"a");
	EXPECT_TRUE(enter().empty()); // в середине строки Enter не засчитывается

	(void)type(U"b");
	auto const update = last_update(enter());
	EXPECT_EQ(update.changed_index, 2U);
	EXPECT_EQ(update.changed_char.status, CharStatus::Correct);
	EXPECT_EQ(update.cursor_position, 3U);
	EXPECT_EQ(update.metrics.keystrokes, 3U);
}

TEST_F(SessionEngineTest, IndentationIsSkippedAndNotCountedAsTyped)
{
	(void)engine_.handle(start_free(U"a\n    b"));
	(void)type(U"a");

	auto const updates = events_of<StateUpdate>(enter());
	ASSERT_EQ(updates.size(), 5U); // перевод строки и четыре пробела отступа
	EXPECT_EQ(updates.back().cursor_position, 6U);
	EXPECT_EQ(updates.back().changed_char.status, CharStatus::Correct);

	auto const last = last_update(type(U"b"));
	ASSERT_TRUE(last.is_completed);
	// Верно набраны три символа за 0.4 с: отступ не ускоряет и не замедляет.
	EXPECT_EQ(last.metrics.keystrokes, 3U);
	EXPECT_NEAR(last.metrics.cpm, 3.0 / 0.4 * 60.0, 1e-9);
}

TEST_F(SessionEngineTest, BackspaceErasesIndentationWithLineBreak)
{
	(void)engine_.handle(start_free(U"a\n  b"));
	(void)type(U"a");
	(void)enter();

	auto const updates = events_of<StateUpdate>(engine_.handle(control(ControlKey::Backspace)));
	ASSERT_EQ(updates.size(), 3U);
	for (auto const& update : updates)
		EXPECT_EQ(update.changed_char.status, CharStatus::Pending);
	EXPECT_EQ(updates.back().cursor_position, 1U);

	// Перевод строки набирается заново, и отступ снова проходится сам.
	EXPECT_EQ(last_update(enter()).cursor_position, 4U);
}

TEST_F(SessionEngineTest, WrongKeyAtLineEndMovesToNextLine)
{
	(void)engine_.handle(start_free(U"a\n b"));
	(void)type(U"a");

	auto const updates = events_of<StateUpdate>(type(U" ")); // пробел вместо Enter
	ASSERT_EQ(updates.size(), 2U);
	EXPECT_EQ(updates.front().changed_char.status, CharStatus::Wrong);
	EXPECT_EQ(updates.back().cursor_position, 3U);
}

TEST_F(SessionEngineTest, FirstLineIndentationIsSkippedAtStart)
{
	auto const states = events_of<SessionState>(engine_.handle(start_free(U"  a\nb")));
	ASSERT_EQ(states.size(), 1U);
	EXPECT_EQ(states[0].cursor_position, 2U);
	EXPECT_TRUE(engine_.handle(control(ControlKey::Backspace)).empty()); // стирать нечего
}

TEST_F(SessionEngineTest, BlankCustomTextDoesNotStartSession)
{ EXPECT_TRUE(engine_.handle(start_free(U" \n ")).empty()); }

TEST_F(SessionEngineTest, CompletionReportsSessionResult)
{
	(void)engine_.handle(start_free(U"привет мир"));
	auto const events = type(U"привет мир");

	auto const results = events_of<SessionResult>(events);
	ASSERT_EQ(results.size(), 1U);
	auto const& record = results[0].record;
	EXPECT_EQ(record.mode, TrainingMode::Free);
	EXPECT_EQ(record.language, Language::Russian); // свободный текст - язык по буквам
	EXPECT_EQ(record.length, 10U);
	EXPECT_EQ(record.errors, 0U);
	EXPECT_DOUBLE_EQ(record.metrics.wpm, last_update(events).metrics.wpm);
	EXPECT_FALSE(results[0].is_personal_best); // сравнивать не с чем
}

TEST_F(SessionEngineTest, ResultNamesWeakestLettersOfTheSession)
{
	std::u32string const text = U"abababababababababab";
	(void)engine_.handle(start_free(text));

	SessionResult result;
	for (char32_t const ch : text)
	{
		now_ += milliseconds(ch == U'b' ? 600 : 150); // «b» даётся тяжело
		auto const results = events_of<SessionResult>(engine_.handle(key(ch, now_)));
		if (!results.empty()) result = results.back();
	}

	ASSERT_FALSE(result.weakest.empty());
	EXPECT_LE(result.weakest.size(), 5U);
	EXPECT_EQ(result.weakest.front().gram.back(), U'b');
}

TEST_F(SessionEngineTest, FasterSessionIsPersonalBest)
{
	auto const run = [this](milliseconds step) {
		(void)engine_.handle(start_free(U"abcdef"));
		return events_of<SessionResult>(type(U"abcdef", step)).at(0);
	};

	EXPECT_FALSE(run(milliseconds(300)).is_personal_best);
	EXPECT_TRUE(run(milliseconds(200)).is_personal_best);
	EXPECT_FALSE(run(milliseconds(250)).is_personal_best);
}

TEST_F(SessionEngineTest, RestartRepeatsTheSameSmartText)
{
	SessionConfig config;
	config.mode          = TrainingMode::Smart;
	config.target_length = 80;

	auto const first
	    = events_of<SessionState>(engine_.handle(StartSessionCommand{.config = config}));
	(void)type(U"xyz");
	auto const second = events_of<SessionState>(engine_.handle(RestartSessionCommand{}));

	ASSERT_EQ(first.size(), 1U);
	ASSERT_EQ(second.size(), 1U);
	ASSERT_EQ(second[0].chars.size(), first[0].chars.size());
	for (std::size_t i = 0; i < first[0].chars.size(); ++i)
	{
		EXPECT_EQ(second[0].chars[i].character, first[0].chars[i].character);
		EXPECT_EQ(second[0].chars[i].status, CharStatus::Pending);
	}
	EXPECT_EQ(second[0].status, SessionStatus::Active);
}

TEST_F(SessionEngineTest, RestartWithoutPreviousSessionDoesNothing)
{ EXPECT_TRUE(engine_.handle(RestartSessionCommand{}).empty()); }

TEST_F(SessionEngineTest, StatisticsSnapshotContainsHistoryAndWeakLetters)
{
	for (int i = 0; i < 3; ++i)
	{
		(void)engine_.handle(start_free(U"the quick brown fox"));
		(void)type(U"the quick brown fox");
	}

	auto const snapshots
	    = events_of<StatisticsSnapshot>(engine_.handle(RequestStatisticsCommand{}));
	ASSERT_EQ(snapshots.size(), 1U);
	EXPECT_EQ(snapshots[0].history.size(), 3U);
	ASSERT_FALSE(snapshots[0].weakest.empty());
	for (auto const& report : snapshots[0].weakest)
		for (char32_t const ch : report.gram)
			EXPECT_TRUE(is_letter(ch)) << "в отчёте не только буквы";
}

TEST_F(SessionEngineTest, ResetClearsStatisticsAndHistory)
{
	(void)engine_.handle(start_free(U"hello world"));
	(void)type(U"hello world");

	auto const snapshots = events_of<StatisticsSnapshot>(engine_.handle(ResetStatisticsCommand{}));
	ASSERT_EQ(snapshots.size(), 1U);
	EXPECT_TRUE(snapshots[0].history.empty());
	EXPECT_TRUE(snapshots[0].weakest.empty());
}

TEST(SessionEnginePersistence, StatisticsAndHistorySurviveRestart)
{
	test::TempDir const dir;
	auto                now = Clock::now();

	{
		SessionEngine engine(dir.path());
		engine.load();
		(void)engine.handle(start_free(U"ok ok ok ok ok ok"));
		for (char32_t const ch : std::u32string(U"ok ok ok ok ok ok"))
			(void)engine.handle(key(ch, now += milliseconds(150)));
	}

	EXPECT_TRUE(std::filesystem::exists(dir.path() / "ngram_stats.json"));
	EXPECT_TRUE(std::filesystem::exists(dir.path() / "history.json"));

	SessionEngine reloaded(dir.path());
	reloaded.load();
	auto const snapshots
	    = events_of<StatisticsSnapshot>(reloaded.handle(RequestStatisticsCommand{}));
	ASSERT_EQ(snapshots.size(), 1U);
	EXPECT_EQ(snapshots[0].history.size(), 1U);
	EXPECT_FALSE(snapshots[0].weakest.empty());
}

} // namespace
} // namespace typing_trainer
