use log::{Metadata, Record};

// const LOGGER: OnceCell<CallbackLogger<Box<dyn Fn(LogLevel, &str)>>> = OnceCell::new();

/// Set the callback function that is called when logging.
///
/// The `callback` function's first parameter is the level of the message being
/// logged, and the second is the message itself.
pub fn set_logging_callback<T>(callback: T)
where
    T: Fn(LogLevel, &str) + Send + Sync + 'static,
{
    // FIXME: set_boxed_logger can only be called once, and it's not possible to retrieve and downcast the logger from log once set.
    let logger = Box::new(CallbackLogger { callback });

    log::set_boxed_logger(logger)
        .map(|_| log::set_max_level(log::LevelFilter::Trace))
        .unwrap();
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

impl From<log::Level> for LogLevel {
    fn from(value: log::Level) -> Self {
        match value {
            log::Level::Trace => LogLevel::Trace,
            log::Level::Debug => LogLevel::Debug,
            log::Level::Info => LogLevel::Info,
            log::Level::Warn => LogLevel::Warning,
            log::Level::Error => LogLevel::Error,
        }
    }
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
struct CallbackLogger<T: Fn(LogLevel, &str)> {
    callback: T,
}

impl<T: Fn(LogLevel, &str) + Send + Sync> log::Log for CallbackLogger<T> {
    fn enabled(&self, _metadata: &Metadata) -> bool {
        true
    }

    fn log(&self, record: &Record) {
        if self.enabled(record.metadata()) {
            (self.callback)(record.level().into(), &format!("{}", record.args()));
        }
    }

    fn flush(&self) {}
}
