

#include <gtest/gtest.h>

#include "libloot-cpp/src/lib.rs.h"
#include "rust/cxx.h"

namespace rust {
template<typename T>
bool operator==(const Vec<T>& lhs, const Vec<T>& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}
}

namespace loot::rust {
TEST(libloot_version, shouldReturnExpectedValue) {
  auto version = libloot_version();

  EXPECT_EQ(version, "0.26.1");
}

TEST(libloot_revision, shouldReturnExpectedValue) {
  auto revision = libloot_revision();

  EXPECT_EQ(revision, "unknown");
}

TEST(new_game, shouldThrowIfGivenNonsense) {
  EXPECT_THROW(new_game(GameType::Fallout3, "foo"), ::rust::Error);
}

TEST(Message, creation) {
  auto content = new_message_content(
      "a message",
      ::rust::String(std::string(message_content_default_language())));

  auto message = new_message(MessageType::Say, "message2", "invalid condition");

  std::vector<::rust::Box<MessageContent>> contents;
  contents.push_back(std::move(content));
  auto multi_message = multilingual_message(
      MessageType::Say,
      ::rust::Slice<const ::rust::Box<MessageContent>>(contents),
      "invalid condition");

  EXPECT_EQ(multi_message->content()[0].text(), "a message");
  EXPECT_EQ(multi_message->content()[0].language(), "en");
  EXPECT_EQ(MessageType::Say, multi_message->message_type());
  EXPECT_EQ(multi_message->condition(), "invalid condition");
}
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
