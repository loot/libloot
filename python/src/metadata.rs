use std::hash::{DefaultHasher, Hash, Hasher};

use libloot_ffi_errors::UnsupportedEnumValueError;
use pyo3::{
    Bound, FromPyObject, PyResult, pyclass, pyfunction, pymethods,
    types::{PyAnyMethods, PyTypeMethods},
};

use crate::error::VerboseError;

pub(crate) const NONE_REPR: &str = "None";

#[pyclass(eq, ord, frozen, hash, str = "{0:?}")]
#[repr(transparent)]
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct Group(libloot::metadata::Group);

#[pymethods]
impl Group {
    #[classattr]
    fn default_name() -> &'static str {
        libloot::metadata::Group::DEFAULT_NAME
    }

    #[new]
    #[pyo3(signature = (name, description = None, after_groups = None))]
    fn new(name: String, description: Option<String>, after_groups: Option<Vec<String>>) -> Self {
        let mut group = libloot::metadata::Group::new(name);

        if let Some(description) = description {
            group = group.with_description(description);
        }

        if let Some(after_groups) = after_groups {
            group = group.with_after_groups(after_groups);
        }

        Self(group)
    }

    #[getter]
    fn name(&self) -> &str {
        self.0.name()
    }

    #[getter]
    fn description(&self) -> Option<&str> {
        self.0.description()
    }

    #[getter]
    fn after_groups(&self) -> &[String] {
        self.0.after_groups()
    }

    fn __repr__(slf: &Bound<'_, Self>) -> PyResult<String> {
        let class_name = slf.get_type().qualname()?;
        let inner = &slf.borrow().0;
        Ok(format!(
            "{}({}, {}, [{}])",
            class_name,
            inner.name(),
            inner.description().unwrap_or(NONE_REPR),
            inner.after_groups().join(",")
        ))
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

#[pyclass(eq, ord, frozen, hash, str = "{0:?}")]
#[repr(transparent)]
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct MessageContent(libloot::metadata::MessageContent);

#[pymethods]
impl MessageContent {
    #[classattr]
    fn default_language() -> &'static str {
        libloot::metadata::MessageContent::DEFAULT_LANGUAGE
    }

    #[new]
    #[pyo3(signature = (text, language = None))]
    fn new(text: String, language: Option<String>) -> Self {
        let mut content = libloot::metadata::MessageContent::new(text);

        if let Some(language) = language {
            content = content.with_language(language);
        }

        Self(content)
    }

    #[getter]
    fn text(&self) -> &str {
        self.0.text()
    }

    #[getter]
    fn language(&self) -> &str {
        self.0.language()
    }

    fn __repr__(slf: &Bound<'_, Self>) -> PyResult<String> {
        let class_name = slf.get_type().qualname()?;
        let inner = &slf.borrow().0;
        Ok(format!(
            "{}({}, {})",
            class_name,
            inner.text(),
            inner.language()
        ))
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

fn repr_message_contents(contents: &[libloot::metadata::MessageContent]) -> String {
    contents
        .iter()
        .map(|c| format!("MessageContent({}, {})", c.text(), c.language()))
        .collect::<Vec<_>>()
        .join(",")
}

#[expect(
    unreachable_pub,
    reason = "It's exported by PyO3, so while the pub isn't necessary, it's misleading to not have it"
)]
#[pyfunction]
pub fn select_message_content(
    content: Vec<MessageContent>,
    language: &str,
) -> Option<MessageContent> {
    let content: Vec<_> = content.into_iter().map(Into::into).collect();
    libloot::metadata::select_message_content(&content, language)
        .cloned()
        .map(Into::into)
}

#[pyclass(eq, frozen, hash, ord)]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
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

#[pyclass(eq, ord, frozen, hash, str = "{0:?}")]
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[repr(transparent)]
pub struct Message(libloot::metadata::Message);

#[derive(FromPyObject)]
enum MessageContents {
    Monolingual(String),
    Multilingual(Vec<MessageContent>),
}

#[pymethods]
impl Message {
    #[new]
    #[pyo3(signature = (message_type, contents, condition = None))]
    fn new(
        message_type: MessageType,
        contents: MessageContents,
        condition: Option<String>,
    ) -> Result<Self, VerboseError> {
        let mut message = match contents {
            MessageContents::Monolingual(c) => {
                libloot::metadata::Message::new(message_type.into(), c)
            }
            MessageContents::Multilingual(c) => {
                let c = c.into_iter().map(Into::into).collect();
                libloot::metadata::Message::multilingual(message_type.into(), c)?
            }
        };

        if let Some(condition) = condition {
            message = message.with_condition(condition);
        }

        Ok(Self(message))
    }

    #[getter]
    fn message_type(&self) -> MessageType {
        self.0.message_type().into()
    }

    #[getter]
    fn content(&self) -> Vec<MessageContent> {
        self.0.content().iter().cloned().map(Into::into).collect()
    }

    #[getter]
    fn condition(&self) -> Option<&str> {
        self.0.condition()
    }

    fn __repr__(slf: &Bound<'_, Self>) -> PyResult<String> {
        let class_name = slf.get_type().qualname()?;
        let inner = &slf.borrow().0;
        Ok(format!(
            "{}({}, [{}], {})",
            class_name,
            inner.message_type(),
            repr_message_contents(inner.content()),
            inner.condition().unwrap_or(NONE_REPR),
        ))
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

#[pyclass(eq, ord, frozen, hash, str = "{0:?}")]
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[repr(transparent)]
pub struct File(libloot::metadata::File);

#[pymethods]
impl File {
    #[new]
    #[pyo3(signature = (name, display_name = None, detail = None, condition = None, constraint = None))]
    fn new(
        name: String,
        display_name: Option<String>,
        detail: Option<Vec<MessageContent>>,
        condition: Option<String>,
        constraint: Option<String>,
    ) -> Result<Self, VerboseError> {
        let mut file = libloot::metadata::File::new(name);

        if let Some(display_name) = display_name {
            file = file.with_display_name(display_name);
        }

        if let Some(detail) = detail {
            let detail = detail.into_iter().map(Into::into).collect();
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

    #[getter]
    fn name(&self) -> Filename {
        self.0.name().clone().into()
    }

    #[getter]
    fn display_name(&self) -> Option<&str> {
        self.0.display_name()
    }

    #[getter]
    fn detail(&self) -> Vec<MessageContent> {
        self.0.detail().iter().cloned().map(Into::into).collect()
    }

    #[getter]
    fn condition(&self) -> Option<&str> {
        self.0.condition()
    }

    #[getter]
    fn constraint(&self) -> Option<&str> {
        self.0.constraint()
    }

    fn __repr__(slf: &Bound<'_, Self>) -> PyResult<String> {
        let class_name = slf.get_type().qualname()?;
        let inner = &slf.borrow().0;
        Ok(format!(
            "{}({}, {}, {}, {}, {})",
            class_name,
            inner.name(),
            inner.display_name().unwrap_or(NONE_REPR),
            repr_message_contents(inner.detail()),
            inner.condition().unwrap_or(NONE_REPR),
            inner.constraint().unwrap_or(NONE_REPR)
        ))
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

#[pyclass(eq, ord, frozen, hash, str = "{0:?}")]
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[repr(transparent)]
pub struct Filename(libloot::metadata::Filename);

#[pymethods]
impl Filename {
    #[new]
    fn new(name: String) -> Self {
        Self(libloot::metadata::Filename::new(name))
    }

    fn as_str(&self) -> &str {
        self.0.as_str()
    }

    fn __repr__(slf: &Bound<'_, Self>) -> PyResult<String> {
        let class_name = slf.get_type().qualname()?;
        let inner = &slf.borrow().0;
        Ok(format!("{}({})", class_name, inner.as_str(),))
    }
}

impl From<libloot::metadata::Filename> for Filename {
    fn from(value: libloot::metadata::Filename) -> Self {
        Self(value)
    }
}

#[pyclass(eq, ord, frozen, hash, str = "{0:?}")]
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[repr(transparent)]
pub struct PluginCleaningData(libloot::metadata::PluginCleaningData);

#[pymethods]
impl PluginCleaningData {
    #[new]
    #[pyo3(signature = (crc, cleaning_utility, itm_count = None, deleted_reference_count = None, deleted_navmesh_count = None, detail = None))]
    fn new(
        crc: u32,
        cleaning_utility: String,
        itm_count: Option<u32>,
        deleted_reference_count: Option<u32>,
        deleted_navmesh_count: Option<u32>,
        detail: Option<Vec<MessageContent>>,
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
            let detail = detail.into_iter().map(Into::into).collect();
            data = data.with_detail(detail)?;
        }

        Ok(Self(data))
    }

    #[getter]
    fn crc(&self) -> u32 {
        self.0.crc()
    }

    #[getter]
    fn itm_count(&self) -> u32 {
        self.0.itm_count()
    }

    #[getter]
    fn deleted_reference_count(&self) -> u32 {
        self.0.deleted_reference_count()
    }

    #[getter]
    fn deleted_navmesh_count(&self) -> u32 {
        self.0.deleted_navmesh_count()
    }

    #[getter]
    fn cleaning_utility(&self) -> &str {
        self.0.cleaning_utility()
    }

    #[getter]
    fn detail(&self) -> Vec<MessageContent> {
        self.0.detail().iter().cloned().map(Into::into).collect()
    }

    fn __repr__(slf: &Bound<'_, Self>) -> PyResult<String> {
        let class_name = slf.get_type().qualname()?;
        let inner = &slf.borrow().0;
        Ok(format!(
            "{}({}, {}, {}, {}, {}, {})",
            class_name,
            inner.crc(),
            inner.cleaning_utility(),
            inner.itm_count(),
            inner.deleted_reference_count(),
            inner.deleted_navmesh_count(),
            repr_message_contents(inner.detail())
        ))
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

#[pyclass(eq, frozen, hash, ord)]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum TagSuggestion {
    Addition,
    Removal,
}

impl TryFrom<TagSuggestion> for libloot::metadata::TagSuggestion {
    type Error = UnsupportedEnumValueError;

    fn try_from(value: TagSuggestion) -> Result<Self, Self::Error> {
        match value {
            TagSuggestion::Addition => Ok(libloot::metadata::TagSuggestion::Addition),
            TagSuggestion::Removal => Ok(libloot::metadata::TagSuggestion::Removal),
        }
    }
}

#[pyclass(eq, ord, frozen, hash, str = "{0:?}")]
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[repr(transparent)]
pub struct Tag(libloot::metadata::Tag);

#[pymethods]
impl Tag {
    #[new]
    #[pyo3(signature = (name, suggestion, condition = None))]
    fn new(
        name: String,
        suggestion: TagSuggestion,
        condition: Option<String>,
    ) -> Result<Self, VerboseError> {
        let mut tag = libloot::metadata::Tag::new(name, suggestion.try_into()?);

        if let Some(condition) = condition {
            tag = tag.with_condition(condition);
        }

        Ok(Self(tag))
    }

    #[getter]
    fn name(&self) -> &str {
        self.0.name()
    }

    #[getter]
    fn is_addition(&self) -> bool {
        self.0.is_addition()
    }

    #[getter]
    fn condition(&self) -> Option<&str> {
        self.0.condition()
    }

    fn __repr__(slf: &Bound<'_, Self>) -> PyResult<String> {
        let class_name = slf.get_type().qualname()?;
        let inner = &slf.borrow().0;
        let suggestion = if inner.is_addition() {
            "TagSuggestion.Addition"
        } else {
            "TagSuggestion.Removal"
        };
        Ok(format!(
            "{}({}, {}, {})",
            class_name,
            inner.name(),
            suggestion,
            inner.condition().unwrap_or(NONE_REPR)
        ))
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

#[pyclass(eq, ord, frozen, hash, str = "{0:?}")]
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[repr(transparent)]
pub struct Location(libloot::metadata::Location);

#[pymethods]
impl Location {
    #[new]
    #[pyo3(signature = (url, name = None))]
    fn new(url: String, name: Option<String>) -> Self {
        let mut location = libloot::metadata::Location::new(url);

        if let Some(name) = name {
            location = location.with_name(name);
        }

        Self(location)
    }

    #[getter]
    fn url(&self) -> &str {
        self.0.url()
    }

    #[getter]
    fn name(&self) -> Option<&str> {
        self.0.name()
    }

    fn __repr__(slf: &Bound<'_, Self>) -> PyResult<String> {
        let class_name = slf.get_type().qualname()?;
        let inner = &slf.borrow().0;
        Ok(format!(
            "{}({}, {})",
            class_name,
            inner.url(),
            inner.name().unwrap_or(NONE_REPR)
        ))
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

#[pyclass(eq, ord, str = "{0:?}")]
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[repr(transparent)]
pub struct PluginMetadata(libloot::metadata::PluginMetadata);

#[pymethods]
impl PluginMetadata {
    #[new]
    fn new(name: &str) -> Result<Self, VerboseError> {
        Ok(Self(libloot::metadata::PluginMetadata::new(name)?))
    }

    #[getter]
    fn name(&self) -> &str {
        self.0.name()
    }

    #[getter]
    fn group(&self) -> Option<&str> {
        self.0.group()
    }

    #[getter]
    fn load_after_files(&self) -> Vec<File> {
        self.0
            .load_after_files()
            .iter()
            .cloned()
            .map(Into::into)
            .collect()
    }

    #[getter]
    fn requirements(&self) -> Vec<File> {
        self.0
            .requirements()
            .iter()
            .cloned()
            .map(Into::into)
            .collect()
    }

    #[getter]
    fn incompatibilities(&self) -> Vec<File> {
        self.0
            .incompatibilities()
            .iter()
            .cloned()
            .map(Into::into)
            .collect()
    }

    #[getter]
    fn messages(&self) -> Vec<Message> {
        self.0.messages().iter().cloned().map(Into::into).collect()
    }

    #[getter]
    fn tags(&self) -> Vec<Tag> {
        self.0.tags().iter().cloned().map(Into::into).collect()
    }

    #[getter]
    fn dirty_info(&self) -> Vec<PluginCleaningData> {
        self.0
            .dirty_info()
            .iter()
            .cloned()
            .map(Into::into)
            .collect()
    }

    #[getter]
    fn clean_info(&self) -> Vec<PluginCleaningData> {
        self.0
            .clean_info()
            .iter()
            .cloned()
            .map(Into::into)
            .collect()
    }

    #[getter]
    fn locations(&self) -> Vec<Location> {
        self.0.locations().iter().cloned().map(Into::into).collect()
    }

    #[setter]
    fn set_group(&mut self, group: Option<String>) {
        match group {
            Some(g) => self.0.set_group(g),
            None => self.0.unset_group(),
        }
    }

    #[setter]
    fn set_load_after_files(&mut self, value: Vec<File>) {
        let value = value.into_iter().map(Into::into).collect();
        self.0.set_load_after_files(value);
    }

    #[setter]
    fn set_requirements(&mut self, value: Vec<File>) {
        let value = value.into_iter().map(Into::into).collect();
        self.0.set_requirements(value);
    }

    #[setter]
    fn set_incompatibilities(&mut self, value: Vec<File>) {
        let value = value.into_iter().map(Into::into).collect();
        self.0.set_incompatibilities(value);
    }

    #[setter]
    fn set_messages(&mut self, value: Vec<Message>) {
        let value = value.into_iter().map(Into::into).collect();
        self.0.set_messages(value);
    }

    #[setter]
    fn set_tags(&mut self, value: Vec<Tag>) {
        let value = value.into_iter().map(Into::into).collect();
        self.0.set_tags(value);
    }

    #[setter]
    fn set_dirty_info(&mut self, value: Vec<PluginCleaningData>) {
        let value = value.into_iter().map(Into::into).collect();
        self.0.set_dirty_info(value);
    }

    #[setter]
    fn set_clean_info(&mut self, value: Vec<PluginCleaningData>) {
        let value = value.into_iter().map(Into::into).collect();
        self.0.set_clean_info(value);
    }

    #[setter]
    fn set_locations(&mut self, value: Vec<Location>) {
        let value = value.into_iter().map(Into::into).collect();
        self.0.set_locations(value);
    }

    fn merge_metadata(&mut self, other: &PluginMetadata) {
        self.0.merge_metadata(&other.0);
    }

    fn has_name_only(&self) -> bool {
        self.0.has_name_only()
    }

    fn is_regex_plugin(&self) -> bool {
        self.0.is_regex_plugin()
    }

    fn name_matches(&self, other_name: &str) -> bool {
        self.0.name_matches(other_name)
    }

    fn as_yaml(&self) -> String {
        self.0.as_yaml()
    }

    fn __hash__(&self) -> u64 {
        let mut hasher = DefaultHasher::new();
        self.0.hash(&mut hasher);
        hasher.finish()
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
