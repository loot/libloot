use delegate::delegate;

use crate::{
    EmptyOptionalError, Optional, OptionalRef, UnsupportedEnumValueError, VerboseError,
    ffi::{MessageType, TagSuggestion},
};

impl From<libloot::metadata::MessageType> for MessageType {
    fn from(value: libloot::metadata::MessageType) -> Self {
        match value {
            libloot::metadata::MessageType::Say => MessageType::say,
            libloot::metadata::MessageType::Warn => MessageType::warn,
            libloot::metadata::MessageType::Error => MessageType::error,
        }
    }
}

impl TryFrom<MessageType> for libloot::metadata::MessageType {
    type Error = UnsupportedEnumValueError;

    fn try_from(value: MessageType) -> Result<Self, UnsupportedEnumValueError> {
        match value {
            MessageType::say => Ok(libloot::metadata::MessageType::Say),
            MessageType::warn => Ok(libloot::metadata::MessageType::Warn),
            MessageType::error => Ok(libloot::metadata::MessageType::Error),
            _ => Err(UnsupportedEnumValueError),
        }
    }
}

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct MessageContent(libloot::metadata::MessageContent);

pub fn new_message_content(text: String) -> Box<MessageContent> {
    Box::new(MessageContent(libloot::metadata::MessageContent::new(text)))
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

            pub fn set_language(&mut self, language: String);
        }
    }
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

fn wrap_message_content_ref(unwrapped: &libloot::metadata::MessageContent) -> &MessageContent {
    let v = unwrapped as *const libloot::metadata::MessageContent;
    // SAFETY: MessageContent is a transparent wrapper, so reinterpreting the pointer is safe.
    unsafe {
        let v = v as *const MessageContent;
        &*v
    }
}

fn wrap_message_contents(slice: &[libloot::metadata::MessageContent]) -> &[MessageContent] {
    // SAFETY: This is safe because MessageContent is a transparent wrapper around the libloot type.
    unsafe { std::slice::from_raw_parts(slice.as_ptr().cast(), slice.len()) }
}

fn unwrap_message_contents(slice: &[MessageContent]) -> &[libloot::metadata::MessageContent] {
    // SAFETY: This is safe because MessageContent is a transparent wrapper around the libloot type.
    unsafe { std::slice::from_raw_parts(slice.as_ptr().cast(), slice.len()) }
}

pub type OptionalMessageContentRef = OptionalRef<MessageContent>;

impl OptionalRef<MessageContent> {
    /// # Safety
    ///
    /// This is safe as long as the pointer in the OptionalRef is still valid.
    pub unsafe fn as_ref(&self) -> Result<&MessageContent, EmptyOptionalError> {
        if self.0.is_null() {
            Err(EmptyOptionalError)
        } else {
            unsafe { Ok(&*self.0) }
        }
    }
}

pub fn select_message_content(
    contents: &[MessageContent],
    language: &str,
) -> Box<OptionalMessageContentRef> {
    let option =
        libloot::metadata::select_message_content(unwrap_message_contents(contents), language);

    Box::new(option.map(wrap_message_content_ref).into())
}

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct Message(libloot::metadata::Message);

pub fn new_message(
    message_type: MessageType,
    content: String,
) -> Result<Box<Message>, VerboseError> {
    Ok(Box::new(Message(libloot::metadata::Message::new(
        message_type.try_into()?,
        content,
    ))))
}

pub fn multilingual_message(
    message_type: MessageType,
    contents: &[Box<MessageContent>],
) -> Result<Box<Message>, VerboseError> {
    let contents = to_vec_of_unwrapped(contents);
    Ok(Box::new(Message(libloot::metadata::Message::multilingual(
        message_type.try_into()?,
        contents,
    )?)))
}

impl Message {
    pub fn condition(&self) -> &str {
        self.0.condition().unwrap_or("")
    }

    pub fn content(&self) -> &[MessageContent] {
        wrap_message_contents(self.0.content())
    }

    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(self.0.clone()))
    }

    delegate! {
        to self.0 {
            #[into]
            pub fn message_type(&self) -> MessageType;

            pub fn set_condition(&mut self, condition: String);
        }
    }
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

fn wrap_messages(slice: &[libloot::metadata::Message]) -> &[Message] {
    // SAFETY: This is safe because Message is a transparent wrapper around the libloot type.
    unsafe { std::slice::from_raw_parts(slice.as_ptr().cast(), slice.len()) }
}

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct Group(libloot::metadata::Group);

pub fn new_group(name: String) -> Box<Group> {
    Box::new(Group(libloot::metadata::Group::new(name)))
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

            pub fn set_description(&mut self, description: String);

            pub fn after_groups(&self) -> &[String];

            pub fn set_after_groups(&mut self, groups: Vec<String>);
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
        wrap_files(self.0.load_after_files())
    }

    pub fn requirements(&self) -> &[File] {
        wrap_files(self.0.requirements())
    }

    pub fn incompatibilities(&self) -> &[File] {
        wrap_files(self.0.incompatibilities())
    }

    pub fn messages(&self) -> &[Message] {
        wrap_messages(self.0.messages())
    }

    pub fn tags(&self) -> &[Tag] {
        wrap_tags(self.0.tags())
    }

    pub fn dirty_info(&self) -> &[PluginCleaningData] {
        wrap_cleaning_data(self.0.dirty_info())
    }

    pub fn clean_info(&self) -> &[PluginCleaningData] {
        wrap_cleaning_data(self.0.clean_info())
    }

    pub fn locations(&self) -> &[Location] {
        wrap_locations(self.0.locations())
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

pub type OptionalPluginMetadata = Optional<PluginMetadata>;

impl From<Option<PluginMetadata>> for Optional<PluginMetadata> {
    fn from(value: Option<PluginMetadata>) -> Self {
        Self(value)
    }
}

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct File(libloot::metadata::File);

pub fn new_file(name: String) -> Box<File> {
    Box::new(File(libloot::metadata::File::new(name)))
}

impl File {
    pub fn filename(&self) -> &Filename {
        let f = self.0.name() as *const libloot::metadata::Filename;
        // SAFETY: Filename is a transparent wrapper, so reinterpreting the pointer is safe.
        unsafe {
            let f = f as *const Filename;
            &*f
        }
    }

    pub fn display_name(&self) -> &str {
        self.0.display_name().unwrap_or("")
    }

    pub fn set_display_name(&mut self, display_name: String) {
        self.0.set_display_name(display_name);
    }

    pub fn detail(&self) -> &[MessageContent] {
        wrap_message_contents(self.0.detail())
    }

    pub fn set_detail(&mut self, detail: &[Box<MessageContent>]) -> Result<(), VerboseError> {
        self.0.set_detail(to_vec_of_unwrapped(detail))?;
        Ok(())
    }

    pub fn condition(&self) -> &str {
        self.0.condition().unwrap_or("")
    }

    pub fn set_condition(&mut self, condition: String) {
        self.0.set_condition(condition);
    }

    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(self.0.clone()))
    }
}

impl From<Box<File>> for libloot::metadata::File {
    fn from(value: Box<File>) -> Self {
        value.0
    }
}

fn wrap_files(slice: &[libloot::metadata::File]) -> &[File] {
    // SAFETY: This is safe because File is a transparent wrapper around the libloot type.
    unsafe { std::slice::from_raw_parts(slice.as_ptr().cast(), slice.len()) }
}

#[derive(Clone, Debug)]
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

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct Tag(libloot::metadata::Tag);

pub fn new_tag(name: String, suggestion: TagSuggestion) -> Result<Box<Tag>, VerboseError> {
    Ok(Box::new(Tag(libloot::metadata::Tag::new(
        name,
        suggestion.try_into()?,
    ))))
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

            pub fn set_condition(&mut self, condition: String);
        }
    }
}

impl From<Box<Tag>> for libloot::metadata::Tag {
    fn from(value: Box<Tag>) -> Self {
        value.0
    }
}

fn wrap_tags(slice: &[libloot::metadata::Tag]) -> &[Tag] {
    // SAFETY: This is safe because Tag is a transparent wrapper around the libloot type.
    unsafe { std::slice::from_raw_parts(slice.as_ptr().cast(), slice.len()) }
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

pub fn new_plugin_cleaning_data(crc: u32, cleaning_utility: String) -> Box<PluginCleaningData> {
    Box::new(PluginCleaningData(
        libloot::metadata::PluginCleaningData::new(crc, cleaning_utility),
    ))
}

impl PluginCleaningData {
    pub fn detail(&self) -> &[MessageContent] {
        wrap_message_contents(self.0.detail())
    }

    pub fn set_detail(&mut self, detail: &[Box<MessageContent>]) -> Result<(), VerboseError> {
        self.0.set_detail(to_vec_of_unwrapped(detail))?;
        Ok(())
    }

    pub fn boxed_clone(&self) -> Box<Self> {
        Box::new(Self(self.0.clone()))
    }

    delegate! {
        to self.0 {
            pub fn crc(&self) -> u32;

            pub fn itm_count(&self) -> u32;

            pub fn set_itm_count(&mut self, count: u32);

            pub fn deleted_reference_count(&self) -> u32;

            pub fn set_deleted_reference_count(&mut self, count: u32);

            pub fn deleted_navmesh_count(&self) -> u32;

            pub fn set_deleted_navmesh_count(&mut self, count: u32);

            pub fn cleaning_utility(&self) -> &str;
        }
    }
}

impl From<Box<PluginCleaningData>> for libloot::metadata::PluginCleaningData {
    fn from(value: Box<PluginCleaningData>) -> Self {
        value.0
    }
}

fn wrap_cleaning_data(slice: &[libloot::metadata::PluginCleaningData]) -> &[PluginCleaningData] {
    // SAFETY: This is safe because PluginCleaningData is a transparent wrapper around the libloot type.
    unsafe { std::slice::from_raw_parts(slice.as_ptr().cast(), slice.len()) }
}

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct Location(libloot::metadata::Location);

pub fn new_location(url: String) -> Box<Location> {
    Box::new(Location(libloot::metadata::Location::new(url)))
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

            pub fn set_name(&mut self, name: String);
        }
    }
}

impl From<Box<Location>> for libloot::metadata::Location {
    fn from(value: Box<Location>) -> Self {
        value.0
    }
}

fn wrap_locations(slice: &[libloot::metadata::Location]) -> &[Location] {
    // SAFETY: This is safe because Location is a transparent wrapper around the libloot type.
    unsafe { std::slice::from_raw_parts(slice.as_ptr().cast(), slice.len()) }
}

pub fn to_vec_of_unwrapped<T: Clone, U: From<Box<T>>>(slice: &[Box<T>]) -> Vec<U> {
    slice.iter().cloned().map(Into::into).collect()
}
