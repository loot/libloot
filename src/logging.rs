use std::sync::{LazyLock, RwLock};

type Callback = dyn Fn(LogLevel, &str) + Send + Sync;

pub(crate) static LOGGER: LazyLock<RwLock<Logger>> =
    LazyLock::new(|| RwLock::new(Logger::new(Box::new(|_, _| {}))));

/// Set the callback function that is called when logging.
///
/// The `callback` function's first parameter is the level of the message being
/// logged, and the second is the message itself.
pub fn set_logging_callback<T>(callback: T)
where
    T: Fn(LogLevel, &str) + Send + Sync + 'static,
{
    let boxed = Box::new(callback);

    match LOGGER.write() {
        Ok(mut logger) => logger.set_callback(boxed),
        Err(e) => {
            e.into_inner().set_callback(boxed);
            LOGGER.clear_poison();
        }
    }
}

// Set the log severity level.
//
// The default level setting is trace. This function has no effect if no logging callback has been set.
pub fn set_log_level(level: LogLevel) {
    match LOGGER.write() {
        Ok(mut logger) => logger.set_level(level),
        Err(e) => {
            e.into_inner().set_level(level);
            LOGGER.clear_poison();
        }
    }
}

/// Codes used to specify different levels of API logging.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Fatal,
}

impl std::fmt::Display for LogLevel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            LogLevel::Trace => write!(f, "trace"),
            LogLevel::Debug => write!(f, "debug"),
            LogLevel::Info => write!(f, "info"),
            LogLevel::Warning => write!(f, "warning"),
            LogLevel::Error => write!(f, "error"),
            LogLevel::Fatal => write!(f, "fatal"),
        }
    }
}

impl From<LogLevel> for log::Level {
    fn from(value: LogLevel) -> Self {
        match value {
            LogLevel::Trace => log::Level::Trace,
            LogLevel::Debug => log::Level::Debug,
            LogLevel::Info => log::Level::Info,
            LogLevel::Warning => log::Level::Warn,
            LogLevel::Error | LogLevel::Fatal => log::Level::Error,
        }
    }
}

pub(crate) struct Logger {
    callback: Box<Callback>,
    level: LogLevel,
}

impl Logger {
    fn new(callback: Box<Callback>) -> Self {
        Self {
            callback,
            level: LogLevel::Trace,
        }
    }

    pub(crate) fn log(&self, level: LogLevel, message: &str) {
        if level >= self.level {
            (self.callback)(level, message);
        }
    }

    fn level(&self) -> LogLevel {
        self.level
    }

    fn set_callback(&mut self, callback: Box<Callback>) {
        self.callback = callback;
    }

    fn set_level(&mut self, level: LogLevel) {
        self.level = level;
    }
}

#[clippy::format_args]
macro_rules! log {
    ($level:expr, $($arg:tt)+) => {
        // Log using the Rust log crate, as it's probably good to support that.
        log::log!(log::Level::from($level), $($arg)+);

        // Also log using the callback.
        let message = std::fmt::format(format_args!($($arg)+));

        match $crate::logging::LOGGER.read() {
            Ok(logger) => logger.log($level, &message),
            Err(e) => {
                $crate::logging::LOGGER.clear_poison();
                e.into_inner().log($level, &message);
            }
        }
    };
}

#[clippy::format_args]
macro_rules! error {
    ($($arg:tt)+) => { $crate::logging::log!(crate::LogLevel::Error, $($arg)+) };
}

#[clippy::format_args]
macro_rules! warning {
    ($($arg:tt)+) => { $crate::logging::log!(crate::LogLevel::Warning, $($arg)+) };
}

#[clippy::format_args]
macro_rules! info {
    ($($arg:tt)+) => { $crate::logging::log!(crate::LogLevel::Info, $($arg)+) };
}

#[clippy::format_args]
macro_rules! debug {
    ($($arg:tt)+) => { $crate::logging::log!(crate::LogLevel::Debug, $($arg)+) };
}

#[clippy::format_args]
macro_rules! trace {
    ($($arg:tt)+) => { $crate::logging::log!(crate::LogLevel::Trace, $($arg)+) };
}

pub fn is_log_enabled(level: LogLevel) -> bool {
    if log::log_enabled!(level.into()) {
        return true;
    }

    let logger = match LOGGER.read() {
        Ok(logger) => logger,
        Err(e) => {
            LOGGER.clear_poison();
            e.into_inner()
        }
    };

    level >= logger.level()
}

pub(crate) use {debug, error, info, log, trace, warning as warn};

pub(crate) fn format_details<E: std::error::Error>(error: &E) -> String {
    let mut details = error.to_string(); // The display string.
    if let Some(source) = error.source() {
        details += ": ";
        details += &format_details(&source);
    }

    details
}

#[cfg(test)]
mod tests {
    use super::*;

    use std::sync::{Arc, LazyLock, Mutex};

    // Since the callback is a global object, these tests need to be run in
    // series so that one doesn't switch out the callback between another
    // doing the same and trying to use it.
    static TEST_LOCK: Mutex<()> = Mutex::new(());

    mod set_logging_callback {
        use super::*;

        #[test]
        fn should_support_a_function() {
            static MESSAGES: LazyLock<Mutex<Vec<(LogLevel, String)>>> =
                LazyLock::new(|| Mutex::new(Vec::new()));

            fn callback(level: LogLevel, message: &str) {
                if let Ok(mut messages) = MESSAGES.lock() {
                    messages.push((level, message.to_owned()));
                }
            }

            let _lock = TEST_LOCK.lock().unwrap();

            set_logging_callback(callback);

            error!("Test message");

            assert_eq!(
                vec![(LogLevel::Error, "Test message".into())],
                *MESSAGES.lock().unwrap()
            );
        }

        #[test]
        fn should_support_a_closure_with_captured_state() {
            let _lock = TEST_LOCK.lock().unwrap();

            let messages = Arc::new(Mutex::new(Vec::new()));
            let cloned_messages = Arc::clone(&messages);
            let callback = move |level, message: &str| {
                if let Ok(mut messages) = cloned_messages.lock() {
                    messages.push((level, message.to_owned()));
                }
            };

            set_logging_callback(callback);

            error!("Test message");

            assert_eq!(
                vec![(LogLevel::Error, "Test message".into())],
                *messages.lock().unwrap()
            );
        }

        #[test]
        fn set_logging_callback_should_be_callable_multiple_times() {
            static MESSAGES: LazyLock<Mutex<Vec<(LogLevel, String)>>> =
                LazyLock::new(|| Mutex::new(Vec::new()));

            fn callback_fn(level: LogLevel, message: &str) {
                if let Ok(mut messages) = MESSAGES.lock() {
                    messages.push((level, message.to_owned()));
                }
            }

            let _lock = TEST_LOCK.lock().unwrap();

            let callback = |_, _: &str| {};
            set_logging_callback(callback);

            let messages = Arc::new(Mutex::new(Vec::new()));
            let cloned_messages = Arc::clone(&messages);
            let callback = move |level, message: &str| {
                if let Ok(mut messages) = cloned_messages.lock() {
                    messages.push((level, message.to_owned()));
                }
            };

            set_logging_callback(callback);

            error!("Test message");

            assert_eq!(
                vec![(LogLevel::Error, "Test message".into())],
                *messages.lock().unwrap()
            );

            set_logging_callback(callback_fn);

            error!("Test message");

            assert_eq!(
                vec![(LogLevel::Error, "Test message".into())],
                *MESSAGES.lock().unwrap()
            );
        }
    }

    mod set_log_level {
        use super::*;

        #[test]
        fn should_set_the_level_used_to_filter_messages_passed_to_the_callback() {
            static MESSAGES: LazyLock<Mutex<Vec<(LogLevel, String)>>> =
                LazyLock::new(|| Mutex::new(Vec::new()));

            fn callback(level: LogLevel, message: &str) {
                if let Ok(mut messages) = MESSAGES.lock() {
                    messages.push((level, message.to_owned()));
                }
            }

            let _lock = TEST_LOCK.lock().unwrap();

            set_logging_callback(callback);
            set_log_level(LogLevel::Warning);

            error!("Test error");
            info!("Test info");

            assert_eq!(
                vec![(LogLevel::Error, "Test error".into())],
                *MESSAGES.lock().unwrap()
            );
        }
    }

    mod is_log_enabled {
        use super::*;

        #[test]
        fn should_return_true_iff_log_level_is_less_than_or_equal_to_given_level() {
            set_log_level(LogLevel::Warning);

            assert!(!is_log_enabled(LogLevel::Trace));
            assert!(!is_log_enabled(LogLevel::Debug));
            assert!(!is_log_enabled(LogLevel::Info));
            assert!(is_log_enabled(LogLevel::Warning));
            assert!(is_log_enabled(LogLevel::Error));
            assert!(is_log_enabled(LogLevel::Fatal));
        }
    }
}
