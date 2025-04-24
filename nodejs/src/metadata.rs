use napi::Either;
use napi_derive::napi;

use crate::error::VerboseError;

#[napi]
#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct Group(libloot::metadata::Group);

#[napi]
impl Group {
    #[napi]
    pub fn default_name() -> &'static str {
        libloot::metadata::Group::DEFAULT_NAME
    }

    #[napi(constructor)]
    pub fn new(
        name: String,
        description: Option<String>,
        after_groups: Option<Vec<String>>,
    ) -> Self {
        let mut group = libloot::metadata::Group::new(name);

        if let Some(description) = description {
            group = group.with_description(description);
        }

        if let Some(after_groups) = after_groups {
            group = group.with_after_groups(after_groups);
        }

        Self(group)
    }

    #[napi(getter)]
    pub fn name(&self) -> &str {
        self.0.name()
    }

    #[napi(getter)]
    pub fn description(&self) -> Option<&str> {
        self.0.description()
    }

    #[napi(getter)]
    pub fn after_groups(&self) -> Vec<String> {
        self.0.after_groups().to_vec()
    }
}

impl From<libloot::metadata::Group> for Group {
    fn from(value: libloot::metadata::Group) -> Self {
        Self(value)
    }
}

impl From<Group> for libloot::metadata::Group {
    fn from(value: Group) -> Self {
        value.0
    }
}

#[napi]
#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct MessageContent(libloot::metadata::MessageContent);

#[napi]
impl MessageContent {
    #[napi]
    pub fn default_language() -> &'static str {
        libloot::metadata::MessageContent::DEFAULT_LANGUAGE
    }

    #[napi(constructor)]
    pub fn new(text: String, language: Option<String>) -> Self {
        let mut content = libloot::metadata::MessageContent::new(text);

        if let Some(language) = language {
            content = content.with_language(language);
        }

        Self(content)
    }

    #[napi(getter)]
    pub fn text(&self) -> &str {
        self.0.text()
    }

    #[napi(getter)]
    pub fn language(&self) -> &str {
        self.0.language()
    }
}

impl From<libloot::metadata::MessageContent> for MessageContent {
    fn from(value: libloot::metadata::MessageContent) -> Self {
        Self(value)
    }
}

impl From<MessageContent> for libloot::metadata::MessageContent {
    fn from(value: MessageContent) -> Self {
        value.0
    }
}

#[napi]
pub fn select_message_content(
    content: Vec<&MessageContent>,
    language: String,
) -> Option<MessageContent> {
    let content: Vec<_> = content.into_iter().cloned().map(Into::into).collect();
    libloot::metadata::select_message_content(&content, &language)
        .cloned()
        .map(Into::into)
}

#[napi]
#[derive(Debug)]
pub enum MessageType {
    Say,
    Warn,
    Error,
}

impl From<libloot::metadata::MessageType> for MessageType {
    fn from(value: libloot::metadata::MessageType) -> Self {
        match value {
            libloot::metadata::MessageType::Say => MessageType::Say,
            libloot::metadata::MessageType::Warn => MessageType::Warn,
            libloot::metadata::MessageType::Error => MessageType::Error,
        }
    }
}

impl From<MessageType> for libloot::metadata::MessageType {
    fn from(value: MessageType) -> Self {
        match value {
            MessageType::Say => libloot::metadata::MessageType::Say,
            MessageType::Warn => libloot::metadata::MessageType::Warn,
            MessageType::Error => libloot::metadata::MessageType::Error,
        }
    }
}

#[napi]
#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct Message(libloot::metadata::Message);

#[napi]
impl Message {
    #[napi(constructor)]
    pub fn new(
        message_type: MessageType,
        contents: Either<String, Vec<&MessageContent>>,
        condition: Option<String>,
    ) -> Result<Self, VerboseError> {
        let mut message = match contents {
            Either::A(c) => libloot::metadata::Message::new(message_type.into(), c),
            Either::B(c) => {
                let c = c.into_iter().cloned().map(Into::into).collect();
                libloot::metadata::Message::multilingual(message_type.into(), c)?
            }
        };

        if let Some(condition) = condition {
            message = message.with_condition(condition);
        }

        Ok(Self(message))
    }

    #[napi(getter)]
    pub fn message_type(&self) -> MessageType {
        self.0.message_type().into()
    }

    #[napi(getter)]
    pub fn content(&self) -> Vec<MessageContent> {
        self.0.content().iter().cloned().map(Into::into).collect()
    }

    #[napi(getter)]
    pub fn condition(&self) -> Option<&str> {
        self.0.condition()
    }
}

impl From<libloot::metadata::Message> for Message {
    fn from(value: libloot::metadata::Message) -> Self {
        Self(value)
    }
}

impl From<Message> for libloot::metadata::Message {
    fn from(value: Message) -> Self {
        value.0
    }
}

#[napi]
#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct File(libloot::metadata::File);

#[napi]
impl File {
    #[napi(constructor)]
    pub fn new(
        name: String,
        display_name: Option<String>,
        detail: Option<Vec<&MessageContent>>,
        condition: Option<String>,
        constraint: Option<String>,
    ) -> Result<Self, VerboseError> {
        let mut file = libloot::metadata::File::new(name);

        if let Some(display_name) = display_name {
            file = file.with_display_name(display_name);
        }

        if let Some(detail) = detail {
            let detail = detail.into_iter().cloned().map(Into::into).collect();
            file = file.with_detail(detail)?;
        }

        if let Some(condition) = condition {
            file = file.with_condition(condition);
        }

        if let Some(constraint) = constraint {
            file = file.with_constraint(constraint);
        }

        Ok(Self(file))
    }

    #[napi(getter)]
    pub fn name(&self) -> Filename {
        self.0.name().clone().into()
    }

    #[napi(getter)]
    pub fn display_name(&self) -> Option<&str> {
        self.0.display_name()
    }

    #[napi(getter)]
    pub fn detail(&self) -> Vec<MessageContent> {
        self.0.detail().iter().cloned().map(Into::into).collect()
    }

    #[napi(getter)]
    pub fn condition(&self) -> Option<&str> {
        self.0.condition()
    }

    #[napi(getter)]
    pub fn constraint(&self) -> Option<&str> {
        self.0.constraint()
    }
}

impl From<libloot::metadata::File> for File {
    fn from(value: libloot::metadata::File) -> Self {
        Self(value)
    }
}

impl From<File> for libloot::metadata::File {
    fn from(value: File) -> Self {
        value.0
    }
}

#[napi]
#[derive(Debug)]
#[repr(transparent)]
pub struct Filename(libloot::metadata::Filename);

#[napi]
impl Filename {
    #[napi(constructor)]
    pub fn new(name: String) -> Self {
        Self(libloot::metadata::Filename::new(name))
    }

    #[napi]
    pub fn as_str(&self) -> &str {
        self.0.as_str()
    }
}

impl From<libloot::metadata::Filename> for Filename {
    fn from(value: libloot::metadata::Filename) -> Self {
        Self(value)
    }
}

#[napi]
#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct PluginCleaningData(libloot::metadata::PluginCleaningData);

#[napi]
impl PluginCleaningData {
    #[napi(constructor)]
    pub fn new(
        crc: u32,
        cleaning_utility: String,
        itm_count: Option<u32>,
        deleted_reference_count: Option<u32>,
        deleted_navmesh_count: Option<u32>,
        detail: Option<Vec<&MessageContent>>,
    ) -> Result<Self, VerboseError> {
        let mut data = libloot::metadata::PluginCleaningData::new(crc, cleaning_utility);

        if let Some(count) = itm_count {
            data = data.with_itm_count(count);
        }

        if let Some(count) = deleted_reference_count {
            data = data.with_deleted_reference_count(count);
        }

        if let Some(count) = deleted_navmesh_count {
            data = data.with_deleted_navmesh_count(count);
        }

        if let Some(detail) = detail {
            let detail = detail.into_iter().cloned().map(Into::into).collect();
            data = data.with_detail(detail)?;
        }

        Ok(Self(data))
    }

    #[napi(getter)]
    pub fn crc(&self) -> u32 {
        self.0.crc()
    }

    #[napi(getter)]
    pub fn itm_count(&self) -> u32 {
        self.0.itm_count()
    }

    #[napi(getter)]
    pub fn deleted_reference_count(&self) -> u32 {
        self.0.deleted_reference_count()
    }

    #[napi(getter)]
    pub fn deleted_navmesh_count(&self) -> u32 {
        self.0.deleted_navmesh_count()
    }

    #[napi(getter)]
    pub fn cleaning_utility(&self) -> &str {
        self.0.cleaning_utility()
    }

    #[napi(getter)]
    pub fn detail(&self) -> Vec<MessageContent> {
        self.0.detail().iter().cloned().map(Into::into).collect()
    }
}

impl From<libloot::metadata::PluginCleaningData> for PluginCleaningData {
    fn from(value: libloot::metadata::PluginCleaningData) -> Self {
        Self(value)
    }
}

impl From<PluginCleaningData> for libloot::metadata::PluginCleaningData {
    fn from(value: PluginCleaningData) -> Self {
        value.0
    }
}

#[napi]
#[derive(Debug)]
pub enum TagSuggestion {
    Addition,
    Removal,
}

impl From<TagSuggestion> for libloot::metadata::TagSuggestion {
    fn from(value: TagSuggestion) -> Self {
        match value {
            TagSuggestion::Addition => libloot::metadata::TagSuggestion::Addition,
            TagSuggestion::Removal => libloot::metadata::TagSuggestion::Removal,
        }
    }
}

#[napi]
#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct Tag(libloot::metadata::Tag);

#[napi]
impl Tag {
    #[napi(constructor)]
    pub fn new(name: String, suggestion: TagSuggestion, condition: Option<String>) -> Self {
        let mut tag = libloot::metadata::Tag::new(name, suggestion.into());

        if let Some(condition) = condition {
            tag = tag.with_condition(condition);
        }

        Self(tag)
    }

    #[napi(getter)]
    pub fn name(&self) -> &str {
        self.0.name()
    }

    #[napi(getter)]
    pub fn is_addition(&self) -> bool {
        self.0.is_addition()
    }

    #[napi(getter)]
    pub fn condition(&self) -> Option<&str> {
        self.0.condition()
    }
}

impl From<libloot::metadata::Tag> for Tag {
    fn from(value: libloot::metadata::Tag) -> Self {
        Self(value)
    }
}

impl From<Tag> for libloot::metadata::Tag {
    fn from(value: Tag) -> Self {
        value.0
    }
}

#[napi]
#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct Location(libloot::metadata::Location);

#[napi]
impl Location {
    #[napi(constructor)]
    pub fn new(url: String, name: Option<String>) -> Self {
        let mut location = libloot::metadata::Location::new(url);

        if let Some(name) = name {
            location = location.with_name(name);
        }

        Self(location)
    }

    #[napi(getter)]
    pub fn url(&self) -> &str {
        self.0.url()
    }

    #[napi(getter)]
    pub fn name(&self) -> Option<&str> {
        self.0.name()
    }
}

impl From<libloot::metadata::Location> for Location {
    fn from(value: libloot::metadata::Location) -> Self {
        Self(value)
    }
}

impl From<Location> for libloot::metadata::Location {
    fn from(value: Location) -> Self {
        value.0
    }
}

#[napi]
#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct PluginMetadata(libloot::metadata::PluginMetadata);

#[napi]
impl PluginMetadata {
    #[napi(constructor)]
    pub fn new(name: String) -> Result<Self, VerboseError> {
        Ok(Self(libloot::metadata::PluginMetadata::new(&name)?))
    }

    #[napi(getter)]
    pub fn name(&self) -> &str {
        self.0.name()
    }

    #[napi(getter)]
    pub fn group(&self) -> Option<&str> {
        self.0.group()
    }

    #[napi(getter)]
    pub fn load_after_files(&self) -> Vec<File> {
        self.0
            .load_after_files()
            .iter()
            .cloned()
            .map(Into::into)
            .collect()
    }

    #[napi(getter)]
    pub fn requirements(&self) -> Vec<File> {
        self.0
            .requirements()
            .iter()
            .cloned()
            .map(Into::into)
            .collect()
    }

    #[napi(getter)]
    pub fn incompatibilities(&self) -> Vec<File> {
        self.0
            .incompatibilities()
            .iter()
            .cloned()
            .map(Into::into)
            .collect()
    }

    #[napi(getter)]
    pub fn messages(&self) -> Vec<Message> {
        self.0.messages().iter().cloned().map(Into::into).collect()
    }

    #[napi(getter)]
    pub fn tags(&self) -> Vec<Tag> {
        self.0.tags().iter().cloned().map(Into::into).collect()
    }

    #[napi(getter)]
    pub fn dirty_info(&self) -> Vec<PluginCleaningData> {
        self.0
            .dirty_info()
            .iter()
            .cloned()
            .map(Into::into)
            .collect()
    }

    #[napi(getter)]
    pub fn clean_info(&self) -> Vec<PluginCleaningData> {
        self.0
            .clean_info()
            .iter()
            .cloned()
            .map(Into::into)
            .collect()
    }

    #[napi(getter)]
    pub fn locations(&self) -> Vec<Location> {
        self.0.locations().iter().cloned().map(Into::into).collect()
    }

    #[napi(setter)]
    pub fn set_group(&mut self, group: Option<String>) {
        match group {
            Some(g) => self.0.set_group(g),
            None => self.0.unset_group(),
        }
    }

    #[napi(setter)]
    pub fn set_load_after_files(&mut self, value: Vec<&File>) {
        let value = value.into_iter().cloned().map(Into::into).collect();
        self.0.set_load_after_files(value);
    }

    #[napi(setter)]
    pub fn set_requirements(&mut self, value: Vec<&File>) {
        let value = value.into_iter().cloned().map(Into::into).collect();
        self.0.set_requirements(value);
    }

    #[napi(setter)]
    pub fn set_incompatibilities(&mut self, value: Vec<&File>) {
        let value = value.into_iter().cloned().map(Into::into).collect();
        self.0.set_incompatibilities(value);
    }

    #[napi(setter)]
    pub fn set_messages(&mut self, value: Vec<&Message>) {
        let value = value.into_iter().cloned().map(Into::into).collect();
        self.0.set_messages(value);
    }

    #[napi(setter)]
    pub fn set_tags(&mut self, value: Vec<&Tag>) {
        let value = value.into_iter().cloned().map(Into::into).collect();
        self.0.set_tags(value);
    }

    #[napi(setter)]
    pub fn set_dirty_info(&mut self, value: Vec<&PluginCleaningData>) {
        let value = value.into_iter().cloned().map(Into::into).collect();
        self.0.set_dirty_info(value);
    }

    #[napi(setter)]
    pub fn set_clean_info(&mut self, value: Vec<&PluginCleaningData>) {
        let value = value.into_iter().cloned().map(Into::into).collect();
        self.0.set_clean_info(value);
    }

    #[napi(setter)]
    pub fn set_locations(&mut self, value: Vec<&Location>) {
        let value = value.into_iter().cloned().map(Into::into).collect();
        self.0.set_locations(value);
    }

    #[napi]
    pub fn merge_metadata(&mut self, other: &PluginMetadata) {
        self.0.merge_metadata(&other.0);
    }

    #[napi]
    pub fn has_name_only(&self) -> bool {
        self.0.has_name_only()
    }

    #[napi]
    pub fn is_regex_plugin(&self) -> bool {
        self.0.is_regex_plugin()
    }

    #[napi]
    pub fn name_matches(&self, other_name: String) -> bool {
        self.0.name_matches(&other_name)
    }

    #[napi]
    pub fn as_yaml(&self) -> String {
        self.0.as_yaml()
    }
}

impl From<libloot::metadata::PluginMetadata> for PluginMetadata {
    fn from(value: libloot::metadata::PluginMetadata) -> Self {
        Self(value)
    }
}

impl From<PluginMetadata> for libloot::metadata::PluginMetadata {
    fn from(value: PluginMetadata) -> Self {
        value.0
    }
}
