/// An enum representing the different possible types of interactions between
/// plugins or groups.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[non_exhaustive]
pub enum EdgeType {
    Hardcoded,
    MasterFlag,
    Master,
    MasterlistRequirement,
    UserRequirement,
    MasterlistLoadAfter,
    UserLoadAfter,
    MasterlistGroup,
    UserGroup,
    RecordOverlap,
    AssetOverlap,
    TieBreak,
    BlueprintMaster,
}

impl std::fmt::Display for EdgeType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            EdgeType::Hardcoded => write!(f, "Hardcoded"),
            EdgeType::MasterFlag => write!(f, "Master Flag"),
            EdgeType::Master => write!(f, "Master"),
            EdgeType::MasterlistRequirement => write!(f, "Masterlist Requirement"),
            EdgeType::UserRequirement => write!(f, "User Requirement"),
            EdgeType::MasterlistLoadAfter => write!(f, "Masterlist Load After"),
            EdgeType::UserLoadAfter => write!(f, "User Load After"),
            EdgeType::MasterlistGroup => write!(f, "Masterlist Group"),
            EdgeType::UserGroup => write!(f, "User Group"),
            EdgeType::RecordOverlap => write!(f, "Record Overlap"),
            EdgeType::AssetOverlap => write!(f, "Asset Overlap"),
            EdgeType::TieBreak => write!(f, "Tie Break"),
            EdgeType::BlueprintMaster => write!(f, "Blueprint Master"),
        }
    }
}

/// Represents a plugin or group vertex in a path, and the type of the edge to
/// the next vertex in the path if one exists.
#[derive(Clone, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct Vertex {
    name: String,
    out_edge_type: Option<EdgeType>,
}

impl Vertex {
    /// Construct a Vertex with the given name and no out edge.
    #[must_use]
    pub fn new(name: String) -> Self {
        Self {
            name,
            ..Default::default()
        }
    }

    /// Set the type of the edge going from this vertex to the next in the path.
    #[must_use]
    pub fn with_out_edge_type(mut self, out_edge_type: EdgeType) -> Self {
        self.set_out_edge_type(out_edge_type);
        self
    }

    /// Get the name of the plugin or group that the vertex represents.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Get the type of the edge going from this vertex to the next in the path.
    pub fn out_edge_type(&self) -> Option<EdgeType> {
        self.out_edge_type
    }

    /// Set the type of the edge going from this vertex to the next in the path.
    pub fn set_out_edge_type(&mut self, out_edge_type: EdgeType) -> &mut Self {
        self.out_edge_type = Some(out_edge_type);
        self
    }
}
