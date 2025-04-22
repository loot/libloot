use std::fmt::Display;

use crate::{Vertex, plugin::error::PluginDataError};

#[derive(Clone, Default, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct UndefinedGroupError {
    group_name: String,
}

impl UndefinedGroupError {
    pub(crate) fn new(group_name: String) -> Self {
        Self { group_name }
    }

    pub(crate) fn into_group_name(self) -> String {
        self.group_name
    }
}

impl Display for UndefinedGroupError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "the group \"{}\" does not exist", self.group_name)
    }
}

impl std::error::Error for UndefinedGroupError {}

#[derive(Clone, Default, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct CyclicInteractionError {
    cycle: Vec<Vertex>,
}

impl CyclicInteractionError {
    pub(crate) fn new(cycle: Vec<Vertex>) -> Self {
        Self { cycle }
    }

    pub(crate) fn into_cycle(self) -> Vec<Vertex> {
        self.cycle
    }
}

impl Display for CyclicInteractionError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let cycle = display_cycle(&self.cycle);
        write!(f, "cyclic interaction detected: {cycle}")
    }
}

impl std::error::Error for CyclicInteractionError {}

pub(crate) fn display_cycle(cycle: &[Vertex]) -> String {
    cycle
        .iter()
        .map(|v| {
            if let Some(edge_type) = v.out_edge_type() {
                format!("{} --[{}]-> ", v.name(), edge_type)
            } else {
                v.name().to_owned()
            }
        })
        .chain(cycle.first().iter().map(|v| v.name().to_owned()))
        .collect()
}

/// Represents an error that occurred while trying to get the path between two
/// groups across the graph formed from group metadata.
#[derive(Debug)]
pub enum GroupsPathError {
    UndefinedGroup(String),
    CycleFound(Vec<Vertex>),
    PathfindingError(Box<dyn std::error::Error + Send + Sync + 'static>),
}

impl Display for GroupsPathError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::UndefinedGroup(g) => write!(f, "the group \"{g}\" does not exist"),
            Self::CycleFound(c) => write!(f, "found a cycle: {}", display_cycle(c)),
            Self::PathfindingError(_) => write!(f, "failed to find a path in the groups graph"),
        }
    }
}

impl std::error::Error for GroupsPathError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::PathfindingError(e) => Some(e.as_ref()),
            _ => None,
        }
    }
}

impl From<BuildGroupsGraphError> for GroupsPathError {
    fn from(value: BuildGroupsGraphError) -> Self {
        match value {
            BuildGroupsGraphError::CycleFound(e) => Self::CycleFound(e.into_cycle()),
            BuildGroupsGraphError::UndefinedGroup(e) => Self::UndefinedGroup(e.into_group_name()),
        }
    }
}

impl From<UndefinedGroupError> for GroupsPathError {
    fn from(value: UndefinedGroupError) -> Self {
        Self::UndefinedGroup(value.into_group_name())
    }
}

impl From<PathfindingError> for GroupsPathError {
    fn from(value: PathfindingError) -> Self {
        Self::PathfindingError(Box::new(value))
    }
}

#[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub(crate) enum BuildGroupsGraphError {
    UndefinedGroup(UndefinedGroupError),
    CycleFound(CyclicInteractionError),
}

impl Display for BuildGroupsGraphError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::UndefinedGroup(_) => write!(f, "encountered an undefined group"),
            Self::CycleFound(_) => write!(f, "the groups graph is cyclic"),
        }
    }
}

impl std::error::Error for BuildGroupsGraphError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::UndefinedGroup(e) => Some(e),
            Self::CycleFound(e) => Some(e),
        }
    }
}

impl From<UndefinedGroupError> for BuildGroupsGraphError {
    fn from(value: UndefinedGroupError) -> Self {
        BuildGroupsGraphError::UndefinedGroup(value)
    }
}

impl From<CyclicInteractionError> for BuildGroupsGraphError {
    fn from(value: CyclicInteractionError) -> Self {
        BuildGroupsGraphError::CycleFound(value)
    }
}

#[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub(crate) enum PathfindingError {
    NegativeCycle,
    PrecedingNodeNotFound(String),
    FollowingNodeNotFound(String),
    EdgeNotFound {
        from_group: String,
        to_group: String,
    },
}

impl Display for PathfindingError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::NegativeCycle => write!(
                f,
                "encountered a cycle of user-defined \"load after\" relationships"
            ),
            Self::PrecedingNodeNotFound(n) => write!(
                f,
                "unexpectedly could not find the node before \"{n}\" in the path that was found",
            ),
            Self::FollowingNodeNotFound(n) => write!(
                f,
                "unexpectedly could not find the node after \"{n}\" in the path that was found",
            ),
            Self::EdgeNotFound {
                from_group,
                to_group,
            } => write!(
                f,
                "unexpectedly could not find the edge going from \"{from_group}\" to \"{to_group}\"",
            ),
        }
    }
}

impl std::error::Error for PathfindingError {}

#[derive(Debug)]
pub(crate) enum PluginGraphValidationError {
    CycleFound(CyclicInteractionError),
    PluginDataError(PluginDataError),
}

impl Display for PluginGraphValidationError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::CycleFound(_) => write!(f, "found a cycle in the plugin graph"),
            Self::PluginDataError(_) => write!(f, "failed to read plugin data"),
        }
    }
}

impl std::error::Error for PluginGraphValidationError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::CycleFound(e) => Some(e),
            Self::PluginDataError(e) => Some(e),
        }
    }
}

impl From<CyclicInteractionError> for PluginGraphValidationError {
    fn from(value: CyclicInteractionError) -> Self {
        PluginGraphValidationError::CycleFound(value)
    }
}

impl From<PluginDataError> for PluginGraphValidationError {
    fn from(value: PluginDataError) -> Self {
        PluginGraphValidationError::PluginDataError(value)
    }
}

#[derive(Debug)]
pub(crate) enum SortingError {
    ValidationError(PluginGraphValidationError),
    UndefinedGroup(UndefinedGroupError),
    CycleFound(CyclicInteractionError),
    CycleInvolving(String),
    PluginDataError(PluginDataError),
    PathfindingError(PathfindingError),
}

impl Display for SortingError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::ValidationError(_) => write!(f, "plugin graph validation failed"),
            Self::UndefinedGroup(_) => write!(f, "found an undefined group"),
            Self::CycleFound(_) => write!(f, "found a cycle"),
            Self::CycleInvolving(n) => write!(f, "found a cycle involving \"{n}\""),
            Self::PluginDataError(_) => write!(f, "failed to read plugin data"),
            Self::PathfindingError(_) => write!(f, "failed to find a path in the plugins graph"),
        }
    }
}

impl std::error::Error for SortingError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::ValidationError(e) => Some(e),
            Self::UndefinedGroup(e) => Some(e),
            Self::CycleFound(e) => Some(e),
            Self::CycleInvolving(_) => None,
            Self::PluginDataError(e) => Some(e),
            Self::PathfindingError(e) => Some(e),
        }
    }
}

impl From<PluginGraphValidationError> for SortingError {
    fn from(value: PluginGraphValidationError) -> Self {
        SortingError::ValidationError(value)
    }
}

impl From<UndefinedGroupError> for SortingError {
    fn from(value: UndefinedGroupError) -> Self {
        SortingError::UndefinedGroup(value)
    }
}

impl From<CyclicInteractionError> for SortingError {
    fn from(value: CyclicInteractionError) -> Self {
        SortingError::CycleFound(value)
    }
}

impl From<PluginDataError> for SortingError {
    fn from(value: PluginDataError) -> Self {
        SortingError::PluginDataError(value)
    }
}

impl From<PathfindingError> for SortingError {
    fn from(value: PathfindingError) -> Self {
        SortingError::PathfindingError(value)
    }
}
