use std::sync::{LazyLock, RwLock};

type Callback = dyn Fn(LogLevel, &str) + Send + Sync;

pub(crate) static LOGGER: LazyLock<RwLock<Box<Callback>>> =
    LazyLock::new(|| RwLock::new(Box::new(|_, _| {})));

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
        Ok(mut logger) => *logger = boxed,
        Err(e) => {
            *e.into_inner() = boxed;
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
            LogLevel::Error => log::Level::Error,
            LogLevel::Fatal => log::Level::Error,
        }
    }
}

macro_rules! log {
    ($level:expr, $($arg:tt)+) => {
        // Log using the Rust log crate, as it's probably good to support that.
        log::log!(log::Level::from($level), $($arg)+);

        // Also log using the callback.
        if let Ok(logger) = $crate::logging::LOGGER.read() {
            logger($level, &std::fmt::format(format_args!($($arg)+)));
        }
    };
}

macro_rules! error {
    ($($arg:tt)+) => { $crate::logging::log!(crate::LogLevel::Error, $($arg)+) };
}

macro_rules! warning {
    ($($arg:tt)+) => { $crate::logging::log!(crate::LogLevel::Warning, $($arg)+) };
}

macro_rules! info {
    ($($arg:tt)+) => { $crate::logging::log!(crate::LogLevel::Info, $($arg)+) };
}

macro_rules! debug {
    ($($arg:tt)+) => { $crate::logging::log!(crate::LogLevel::Debug, $($arg)+) };
}

macro_rules! trace {
    ($($arg:tt)+) => { $crate::logging::log!(crate::LogLevel::Trace, $($arg)+) };
}

pub(crate) use {debug, error, info, log, trace, warning as warn};

#[cfg(test)]
mod tests {
    use super::*;

    mod set_logging_callback {
        use super::*;

        use std::sync::{Arc, LazyLock, Mutex};

        // Since the callback is a global object, these tests need to be run in
        // series so that one doesn't switch out the callback between another
        // doing the same and trying to use it.
        static TEST_LOCK: Mutex<()> = Mutex::new(());

        #[test]
        fn should_support_a_function() {
            let _lock = TEST_LOCK.lock().unwrap();

            static MESSAGES: LazyLock<Mutex<Vec<(LogLevel, String)>>> =
                LazyLock::new(|| Mutex::new(Vec::new()));

            fn callback(level: LogLevel, message: &str) {
                if let Ok(mut messages) = MESSAGES.lock() {
                    messages.push((level, message.to_string()));
                }
            }

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

            let messages = Arc::new(Mutex::new(Vec::<(LogLevel, String)>::new()));
            let cloned_messages = messages.clone();
            let callback = move |level, message: &str| {
                if let Ok(mut messages) = cloned_messages.lock() {
                    messages.push((level, message.to_string()));
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
            let _lock = TEST_LOCK.lock().unwrap();

            let callback = |_, _: &str| {};
            set_logging_callback(callback);

            let messages = Arc::new(Mutex::new(Vec::<(LogLevel, String)>::new()));
            let cloned_messages = messages.clone();
            let callback = move |level, message: &str| {
                if let Ok(mut messages) = cloned_messages.lock() {
                    messages.push((level, message.to_string()));
                }
            };

            set_logging_callback(callback);

            error!("Test message");

            assert_eq!(
                vec![(LogLevel::Error, "Test message".into())],
                *messages.lock().unwrap()
            );

            static MESSAGES: LazyLock<Mutex<Vec<(LogLevel, String)>>> =
                LazyLock::new(|| Mutex::new(Vec::new()));

            fn callback_fn(level: LogLevel, message: &str) {
                if let Ok(mut messages) = MESSAGES.lock() {
                    messages.push((level, message.to_string()));
                }
            }

            set_logging_callback(callback_fn);

            error!("Test message");

            assert_eq!(
                vec![(LogLevel::Error, "Test message".into())],
                *MESSAGES.lock().unwrap()
            );
        }
    }
}
