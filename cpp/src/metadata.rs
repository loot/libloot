use delegate::delegate;

use crate::{
    UnsupportedEnumValueError, VerboseError,
    ffi::{MessageType, OptionalMessageContentRef, TagSuggestion},
};

/// # Safety
///
/// Only implement this on structs that are #[repr(transparent)] and that have a
/// single field.
unsafe trait TransparentWrapper {
    type Wrapped;

    fn wrap_ref(value: &Self::Wrapped) -> &Self
    where
        Self: Sized,
    {
        let v: *const Self::Wrapped = value;
        // SAFETY: Reinterpreting the pointer of a transparent wrapper to the type it wraps is safe.
        unsafe {
            let v: *const Self = v.cast();
            &*v
        }
    }

    fn wrap_slice(slice: &[Self::Wrapped]) -> &[Self]
    where
        Self: Sized,
    {
        // SAFETY: This is safe because a transparent wrapper is the same in memory as the type it wraps.
        unsafe { std::slice::from_raw_parts(slice.as_ptr().cast(), slice.len()) }
    }

    fn unwrap_slice(slice: &[Self]) -> &[Self::Wrapped]
    where
        Self: Sized,
    {
        // SAFETY: This is safe because a transparent wrapper is the same in memory as the type it wraps.
        unsafe { std::slice::from_raw_parts(slice.as_ptr().cast(), slice.len()) }
    }
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

impl TryFrom<MessageType> for libloot::metadata::MessageType {
    type Error = UnsupportedEnumValueError;

    fn try_from(value: MessageType) -> Result<Self, UnsupportedEnumValueError> {
        match value {
            MessageType::Say => Ok(libloot::metadata::MessageType::Say),
            MessageType::Warn => Ok(libloot::metadata::MessageType::Warn),
            MessageType::Error => Ok(libloot::metadata::MessageType::Error),
            _ => Err(UnsupportedEnumValueError),
        }
    }
}

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct MessageContent(libloot::metadata::MessageContent);

pub fn new_message_content(text: String, language: &str) -> Box<MessageContent> {
    let mut content = libloot::metadata::MessageContent::new(text);

    if !language.is_empty() {
        content = content.with_language(language.to_owned());
    }

    Box::new(MessageContent(content))
}

pub fn message_content_default_language() -> &'static str {
    libloot::metadata::MessageContent::DEFAULT_LANGUAGE
}

impl MessageContent {
    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(self.0.clone()))
    }

    delegate! {
        to self.0 {
            pub fn text(&self) -> &str;

            pub fn language(&self) -> &str;
        }
    }
}

// SAFETY: MessageContent has #[repr(transparent)]
unsafe impl TransparentWrapper for MessageContent {
    type Wrapped = libloot::metadata::MessageContent;
}

impl From<MessageContent> for libloot::metadata::MessageContent {
    fn from(value: MessageContent) -> Self {
        value.0
    }
}

impl From<Box<MessageContent>> for libloot::metadata::MessageContent {
    fn from(value: Box<MessageContent>) -> Self {
        value.0
    }
}

pub fn select_message_content(
    contents: &[MessageContent],
    language: &str,
) -> OptionalMessageContentRef {
    let option =
        libloot::metadata::select_message_content(MessageContent::unwrap_slice(contents), language);

    option.map(MessageContent::wrap_ref).into()
}

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct Message(libloot::metadata::Message);

pub fn new_message(
    message_type: MessageType,
    content: String,
    condition: &str,
) -> Result<Box<Message>, VerboseError> {
    let mut message = libloot::metadata::Message::new(message_type.try_into()?, content);

    if !condition.is_empty() {
        message = message.with_condition(condition.to_owned());
    }

    Ok(Box::new(Message(message)))
}

pub fn multilingual_message(
    message_type: MessageType,
    contents: &[Box<MessageContent>],
    condition: &str,
) -> Result<Box<Message>, VerboseError> {
    let contents = to_vec_of_unwrapped(contents);

    let mut message = libloot::metadata::Message::multilingual(message_type.try_into()?, contents)?;

    if !condition.is_empty() {
        message = message.with_condition(condition.to_owned());
    }

    Ok(Box::new(Message(message)))
}

impl Message {
    pub fn condition(&self) -> &str {
        self.0.condition().unwrap_or("")
    }

    pub fn content(&self) -> &[MessageContent] {
        MessageContent::wrap_slice(self.0.content())
    }

    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(self.0.clone()))
    }

    delegate! {
        to self.0 {
            #[into]
            pub fn message_type(&self) -> MessageType;
        }
    }
}

// SAFETY: Message has #[repr(transparent)]
unsafe impl TransparentWrapper for Message {
    type Wrapped = libloot::metadata::Message;
}

impl From<libloot::metadata::Message> for Message {
    fn from(value: libloot::metadata::Message) -> Self {
        Self(value)
    }
}

impl From<Box<Message>> for libloot::metadata::Message {
    fn from(value: Box<Message>) -> Self {
        value.0
    }
}

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct Group(libloot::metadata::Group);

pub fn new_group(name: String, description: &str, after_groups: Vec<String>) -> Box<Group> {
    let mut group = libloot::metadata::Group::new(name);

    if !description.is_empty() {
        group = group.with_description(description.to_owned());
    }

    if !after_groups.is_empty() {
        group = group.with_after_groups(after_groups);
    }

    Box::new(Group(group))
}

pub fn group_default_name() -> &'static str {
    libloot::metadata::Group::DEFAULT_NAME
}

impl Group {
    pub fn description(&self) -> &str {
        self.0.description().unwrap_or("")
    }

    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(self.0.clone()))
    }

    delegate! {
        to self.0 {
            pub fn name(&self) -> &str;

            pub fn after_groups(&self) -> &[String];
        }
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

impl From<Box<Group>> for libloot::metadata::Group {
    fn from(value: Box<Group>) -> Self {
        value.0
    }
}

impl From<Box<Group>> for Group {
    fn from(value: Box<Group>) -> Self {
        Self((*value).into())
    }
}

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct PluginMetadata(libloot::metadata::PluginMetadata);

pub fn new_plugin_metadata(name: &str) -> Result<Box<PluginMetadata>, VerboseError> {
    Ok(Box::new(PluginMetadata(
        libloot::metadata::PluginMetadata::new(name)?,
    )))
}

impl PluginMetadata {
    pub fn group(&self) -> &str {
        self.0.group().unwrap_or("")
    }

    pub fn load_after_files(&self) -> &[File] {
        File::wrap_slice(self.0.load_after_files())
    }

    pub fn requirements(&self) -> &[File] {
        File::wrap_slice(self.0.requirements())
    }

    pub fn incompatibilities(&self) -> &[File] {
        File::wrap_slice(self.0.incompatibilities())
    }

    pub fn messages(&self) -> &[Message] {
        Message::wrap_slice(self.0.messages())
    }

    pub fn tags(&self) -> &[Tag] {
        Tag::wrap_slice(self.0.tags())
    }

    pub fn dirty_info(&self) -> &[PluginCleaningData] {
        PluginCleaningData::wrap_slice(self.0.dirty_info())
    }

    pub fn clean_info(&self) -> &[PluginCleaningData] {
        PluginCleaningData::wrap_slice(self.0.clean_info())
    }

    pub fn locations(&self) -> &[Location] {
        Location::wrap_slice(self.0.locations())
    }

    pub fn set_load_after_files(&mut self, files: &[Box<File>]) {
        self.0.set_load_after_files(to_vec_of_unwrapped(files));
    }

    pub fn set_requirements(&mut self, files: &[Box<File>]) {
        self.0.set_requirements(to_vec_of_unwrapped(files));
    }

    pub fn set_incompatibilities(&mut self, files: &[Box<File>]) {
        self.0.set_incompatibilities(to_vec_of_unwrapped(files));
    }

    pub fn set_messages(&mut self, messages: &[Box<Message>]) {
        self.0.set_messages(to_vec_of_unwrapped(messages));
    }

    pub fn set_tags(&mut self, tags: &[Box<Tag>]) {
        self.0.set_tags(to_vec_of_unwrapped(tags));
    }

    pub fn set_dirty_info(&mut self, info: &[Box<PluginCleaningData>]) {
        self.0.set_dirty_info(to_vec_of_unwrapped(info));
    }

    pub fn set_clean_info(&mut self, info: &[Box<PluginCleaningData>]) {
        self.0.set_clean_info(to_vec_of_unwrapped(info));
    }

    pub fn set_locations(&mut self, locations: &[Box<Location>]) {
        self.0.set_locations(to_vec_of_unwrapped(locations));
    }

    pub fn merge_metadata(&mut self, plugin: &PluginMetadata) {
        self.0.merge_metadata(&plugin.0);
    }

    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(self.0.clone()))
    }

    delegate! {
        to self.0 {
            pub fn name(&self) -> &str;

            pub fn set_group(&mut self, group: String);

            pub fn unset_group(&mut self);

            pub fn has_name_only(&self) -> bool;

            pub fn is_regex_plugin(&self) -> bool;

            pub fn name_matches(&self, other_name: &str) -> bool;

            pub fn as_yaml(&self) -> String;
        }
    }
}

impl From<libloot::metadata::PluginMetadata> for PluginMetadata {
    fn from(value: libloot::metadata::PluginMetadata) -> Self {
        Self(value)
    }
}

impl From<Box<PluginMetadata>> for libloot::metadata::PluginMetadata {
    fn from(value: Box<PluginMetadata>) -> Self {
        value.0
    }
}

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct File(libloot::metadata::File);

pub fn new_file(
    name: String,
    display_name: &str,
    condition: &str,
    detail: &[Box<MessageContent>],
    constraint: &str,
) -> Result<Box<File>, VerboseError> {
    let mut file = libloot::metadata::File::new(name);

    if !display_name.is_empty() {
        file = file.with_display_name(display_name.to_owned());
    }

    if !condition.is_empty() {
        file = file.with_condition(condition.to_owned());
    }

    if !detail.is_empty() {
        file = file.with_detail(to_vec_of_unwrapped(detail))?;
    }

    if !constraint.is_empty() {
        file = file.with_constraint(constraint.to_owned());
    }

    Ok(Box::new(File(file)))
}

impl File {
    pub fn filename(&self) -> &Filename {
        Filename::wrap_ref(self.0.name())
    }

    pub fn display_name(&self) -> &str {
        self.0.display_name().unwrap_or("")
    }

    pub fn detail(&self) -> &[MessageContent] {
        MessageContent::wrap_slice(self.0.detail())
    }

    pub fn condition(&self) -> &str {
        self.0.condition().unwrap_or("")
    }

    pub fn constraint(&self) -> &str {
        self.0.constraint().unwrap_or("")
    }

    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(self.0.clone()))
    }
}

// SAFETY: File has #[repr(transparent)]
unsafe impl TransparentWrapper for File {
    type Wrapped = libloot::metadata::File;
}

impl From<Box<File>> for libloot::metadata::File {
    fn from(value: Box<File>) -> Self {
        value.0
    }
}

#[derive(Clone, Debug, PartialEq, PartialOrd)]
#[repr(transparent)]
pub struct Filename(libloot::metadata::Filename);

pub fn new_filename(name: String) -> Box<Filename> {
    Box::new(Filename(libloot::metadata::Filename::new(name)))
}

impl Filename {
    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(self.0.clone()))
    }

    delegate! {
        to self.0 {
            pub fn as_str(&self) -> &str;
        }
    }
}

// SAFETY: Filename has #[repr(transparent)]
unsafe impl TransparentWrapper for Filename {
    type Wrapped = libloot::metadata::Filename;
}

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct Tag(libloot::metadata::Tag);

pub fn new_tag(
    name: String,
    suggestion: TagSuggestion,
    condition: &str,
) -> Result<Box<Tag>, VerboseError> {
    let mut tag = libloot::metadata::Tag::new(name, suggestion.try_into()?);

    if !condition.is_empty() {
        tag = tag.with_condition(condition.to_owned());
    }

    Ok(Box::new(Tag(tag)))
}

impl Tag {
    pub fn condition(&self) -> &str {
        self.0.condition().unwrap_or("")
    }

    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(self.0.clone()))
    }

    delegate! {
        to self.0 {
            pub fn name(&self) -> &str;

            pub fn is_addition(&self) -> bool;
        }
    }
}

// SAFETY: Tag has #[repr(transparent)]
unsafe impl TransparentWrapper for Tag {
    type Wrapped = libloot::metadata::Tag;
}

impl From<Box<Tag>> for libloot::metadata::Tag {
    fn from(value: Box<Tag>) -> Self {
        value.0
    }
}

impl TryFrom<TagSuggestion> for libloot::metadata::TagSuggestion {
    type Error = UnsupportedEnumValueError;

    fn try_from(value: TagSuggestion) -> Result<Self, Self::Error> {
        match value {
            TagSuggestion::Addition => Ok(libloot::metadata::TagSuggestion::Addition),
            TagSuggestion::Removal => Ok(libloot::metadata::TagSuggestion::Removal),
            _ => Err(UnsupportedEnumValueError),
        }
    }
}

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct PluginCleaningData(libloot::metadata::PluginCleaningData);

pub fn new_plugin_cleaning_data(
    crc: u32,
    cleaning_utility: String,
    detail: &[Box<MessageContent>],
    itm_count: u32,
    deleted_reference_count: u32,
    deleted_navmesh_count: u32,
) -> Result<Box<PluginCleaningData>, VerboseError> {
    let mut data = libloot::metadata::PluginCleaningData::new(crc, cleaning_utility)
        .with_itm_count(itm_count)
        .with_deleted_reference_count(deleted_reference_count)
        .with_deleted_navmesh_count(deleted_navmesh_count);

    if !detail.is_empty() {
        data = data.with_detail(to_vec_of_unwrapped(detail))?;
    }

    Ok(Box::new(PluginCleaningData(data)))
}

impl PluginCleaningData {
    pub fn detail(&self) -> &[MessageContent] {
        MessageContent::wrap_slice(self.0.detail())
    }

    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(self.0.clone()))
    }

    delegate! {
        to self.0 {
            pub fn crc(&self) -> u32;

            pub fn itm_count(&self) -> u32;

            pub fn deleted_reference_count(&self) -> u32;

            pub fn deleted_navmesh_count(&self) -> u32;

            pub fn cleaning_utility(&self) -> &str;
        }
    }
}

// SAFETY: PluginCleaningData has #[repr(transparent)]
unsafe impl TransparentWrapper for PluginCleaningData {
    type Wrapped = libloot::metadata::PluginCleaningData;
}

impl From<Box<PluginCleaningData>> for libloot::metadata::PluginCleaningData {
    fn from(value: Box<PluginCleaningData>) -> Self {
        value.0
    }
}

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct Location(libloot::metadata::Location);

pub fn new_location(url: String, name: &str) -> Box<Location> {
    let mut location = libloot::metadata::Location::new(url);

    if !name.is_empty() {
        location = location.with_name(name.to_owned());
    }

    Box::new(Location(location))
}

impl Location {
    pub fn name(&self) -> &str {
        self.0.name().unwrap_or("")
    }

    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(self.0.clone()))
    }

    delegate! {
        to self.0 {
            pub fn url(&self) -> &str;
        }
    }
}

// SAFETY: Location has #[repr(transparent)]
unsafe impl TransparentWrapper for Location {
    type Wrapped = libloot::metadata::Location;
}

impl From<Box<Location>> for libloot::metadata::Location {
    fn from(value: Box<Location>) -> Self {
        value.0
    }
}

pub fn to_vec_of_unwrapped<T: Clone, U: From<Box<T>>>(slice: &[Box<T>]) -> Vec<U> {
    slice.iter().cloned().map(Into::into).collect()
}
