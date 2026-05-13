import streamlit as st
import subprocess
import os

# --- Page Setup & Premium Styling ---
st.set_page_config(page_title="Freelance Hub", page_icon="💼", layout="wide")

# Custom embedded CSS for an elite, colorful UI experience
st.markdown("""
<style>
    /* Premium button styles */
    div.stButton > button:first-child {
        border-radius: 8px;
        font-weight: bold;
        transition: all 0.2s ease-in-out;
    }
    div.stButton > button:first-child:hover {
        transform: translateY(-2px);
        box-shadow: 0 4px 12px rgba(0,0,0,0.15);
    }
    
    /* Beautiful Chat Bubbles */
    .chat-bubble-you {
        background: linear-gradient(135deg, #10B981, #059669);
        color: white;
        padding: 10px 15px;
        border-radius: 18px 18px 0px 18px;
        margin: 8px 0px 8px auto;
        max-width: 75%;
        box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        font-size: 14px;
    }
    .chat-bubble-peer {
        background: linear-gradient(135deg, #3B82F6, #2563EB);
        color: white;
        padding: 10px 15px;
        border-radius: 18px 18px 18px 0px;
        margin: 8px auto 8px 0px;
        max-width: 75%;
        box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        font-size: 14px;
    }
    
    /* Elegant metric card styling */
    div[data-testid="metric-container"] {
        background-color: rgba(255, 255, 255, 0.05);
        border: 1px solid rgba(255, 255, 255, 0.1);
        padding: 15px;
        border-radius: 12px;
        box-shadow: 0 4px 6px rgba(0,0,0,0.05);
    }
</style>
""", unsafe_allow_html=True)

# --- Helper: Render Beautiful HTML Skill Badges ---
def render_tags(tag_string):
    tags = tag_string.split()
    badges = ""
    colors = ["#E0F2FE", "#FEF3C7", "#DCFCE7", "#F3E8FF", "#FFE4E6"]
    text_colors = ["#0369A1", "#B45309", "#15803D", "#6B21A8", "#BE123C"]
    
    for idx, tag in enumerate(tags):
        bg = colors[idx % len(colors)]
        tc = text_colors[idx % len(text_colors)]
        badges += f'<span style="background-color: {bg}; color: {tc}; padding: 3px 10px; border-radius: 12px; font-size: 12px; font-weight: bold; margin-right: 5px; display: inline-block;">{tag}</span>'
    return badges

# --- Connect to C++ Backend & Auto-Compile for Cloud ---
def get_binary_path():
    return './backend.exe' if os.name == 'nt' else './backend'

if 'cpp_core' not in st.session_state:
    binary = get_binary_path()
    
    # Auto-compile logic if binary doesn't exist (essential for Streamlit Community Cloud deployment)
    if not os.path.exists(binary):
        with st.spinner("Compiling C++ Enterprise Core for Cloud Environment... ⚙️"):
            compile_cmd = ["g++", "-o", "backend", "backend.cpp"] if os.name != 'nt' else ["g++", "-o", "backend.exe", "backend.cpp"]
            compile_res = subprocess.run(compile_cmd, capture_output=True, text=True)
            
            if compile_res.returncode != 0:
                st.error(f"⚠️ C++ Compilation Failed:\n{compile_res.stderr}")
                st.stop()
                
    st.session_state.cpp_core = subprocess.Popen(
        [binary], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1
    )

def execute_core_command(csv_command):
    """Sends a flat CSV command to C++ and returns the parsed response list."""
    proc = st.session_state.cpp_core
    try:
        proc.stdin.write(csv_command + "\n")
        proc.stdin.flush()
        response_line = proc.stdout.readline().strip()
        
        if csv_command.startswith("get_msgs"):
            return response_line.split(',', 1)
        elif csv_command in ["gigs_list", "projects_list"]:
            return response_line.split(',', 2)
        else:
            return response_line.split(',')
    except Exception:
        return ["false", "Connection to the platform core was lost. Please refresh the page."]

# --- User Session Memory ---
for key in ['auth_id', 'auth_role', 'auth_name', 'active_chat_peer']:
    if key not in st.session_state:
        st.session_state[key] = None

def logout():
    st.session_state.auth_id = None
    st.session_state.auth_role = None
    st.session_state.auth_name = None
    st.session_state.active_chat_peer = None
    st.success("You have logged out successfully.")

# --- Sidebar: Auth & Enhanced Profile Analytics ---
with st.sidebar:
    st.markdown("## 🚀 **Freelance Hub**")
    st.caption("Find the perfect match for your next project.")
    st.divider()

    if st.session_state.auth_id is None:
        st.info("👋 **Welcome! Please Sign In**")
        auth_tab = st.radio("Choose Mode", ["Sign In", "Create Account"], horizontal=True)
        
        if auth_tab == "Sign In":
            login_uname = st.text_input("Username")
            login_pass = st.text_input("Password", type="password")
            if st.button("Sign In ➔", use_container_width=True):
                if login_uname and login_pass:
                    res = execute_core_command(f"login,{login_uname},{login_pass}")
                    if res[0] == "true":
                        st.session_state.auth_id = int(res[1])
                        st.session_state.auth_name = res[2]
                        st.session_state.auth_role = res[3]
                        st.rerun()
                    else:
                        st.error("Sign in failed. Please verify your username and password.")
        else:
            reg_role = st.selectbox("I want to:", ["Hire Talent (Client)", "Work as a Freelancer"])
            clean_role = "client" if "Client" in reg_role else "freelancer"
            
            reg_name = st.text_input("Choose a Username")
            reg_pass = st.text_input("Create a Password", type="password")
            
            # CONDITIONALLY HIDDEN: Only shown to Clients; Freelancers automatically default to a safe 0.0 deposit
            if clean_role == "client":
                reg_dep = st.number_input("Add Initial Funds ($) [Optional]", min_value=0.0, value=0.0, step=50.0)
            else:
                reg_dep = 0.0
            
            if st.button("Join Marketplace 🎉", use_container_width=True):
                if reg_name and reg_pass:
                    res = execute_core_command(f"register,{clean_role},{reg_name},{reg_pass},{reg_dep}")
                    if res[0] == "true":
                        st.success("Account created successfully! You can now sign in.")
                    else:
                        st.error("Could not create account. That username might already be taken.")
    else:
        # --- Profile Analytics Display ---
        st.markdown(f"### 👋 Hello, **{st.session_state.auth_name}**!")
        role_color = "blue" if st.session_state.auth_role == "Client" else "green"
        st.markdown(f"👤 **Account Type:** :{role_color}[{st.session_state.auth_role}]")
        
        # ADDED DISPLAY: Explicitly displays the logged-in User's ID
        st.markdown(f"🔑 **Account ID:** `{st.session_state.auth_id}`")
        
        prof_res = execute_core_command(f"profile,{st.session_state.auth_id}")
        if prof_res[0] == "true":
            st.metric("Wallet Balance", f"${float(prof_res[5]):,.2f}")
            
            col_rate, col_rev = st.columns(2)
            with col_rate:
                st.metric("Average Rating", f"{float(prof_res[6]):.1f} ⭐")
            with col_rev:
                st.metric("Reviews", prof_res[7])
                
            if st.session_state.auth_role == "Freelancer" and len(prof_res) > 9:
                st.metric("Completed Projects", prof_res[9])
            
        if st.session_state.auth_role == "Client":
            st.divider()
            st.write("#### 💰 Deposit Funds")
            topup_amt = st.number_input("Amount ($)", min_value=10.0, value=100.0, step=50.0)
            if st.button("Add Funds ➔", use_container_width=True):
                res = execute_core_command(f"topup,{st.session_state.auth_id},{topup_amt}")
                if res[0] == "true":
                    st.success("Funds securely added to your wallet!")
                    st.rerun()
                else:
                    st.error("Could not process deposit. Please try again.")
                
        st.divider()
        if st.button("Log Out 🚪", use_container_width=True):
            logout()

# --- Main Dashboard ---
st.title("🌟 Freelance Marketplace")
st.caption("Browse services, hire professionals, and manage safe payments.")

tab_overview, tab_gigs, tab_projects, tab_chat, tab_reviews = st.tabs([
    "📊 Platform Activity", "🛒 Browse Gigs", "📢 View Open Projects", "💬 Messages & Payments", "⭐ Platform Reviews"
])

# Module 1: Vivid Diagnostic Stats
with tab_overview:
    st.subheader("🔥 Current Marketplace Activity")
    execute_core_command("stats") 
    res = execute_core_command("stats")
    if res[0] == "true":
        col1, col2, col3, col4 = st.columns(4)
        col1.metric("👥 Total Users", res[1])
        col2.metric("🏢 Clients", res[2])
        col3.metric("💻 Freelancers", res[3])
        col4.metric("🏷️ Available Gigs", res[4])
    else:
        st.error("Could not load platform stats right now.")

# Module 2: Browse Gigs (Enforces $100 Floor Constraints)
with tab_gigs:
    st.subheader("🚀 Find Freelancers by Skill")
    search_tag = st.text_input("🔍 Search skills", placeholder="e.g., React, Python, UI, Logo...").lower()
    
    execute_core_command("gigs_list") 
    gig_res = execute_core_command("gigs_list")
    
    if gig_res[0] == "true" and int(gig_res[1]) > 0:
        gigs_packed = gig_res[2].split('|') if len(gig_res) > 2 else []
        for gig_str in gigs_packed:
            if not gig_str: continue
            g_fields = gig_str.split(',')
            
            if search_tag and search_tag not in g_fields[5].lower():
                continue
                
            with st.container(border=True):
                g_col1, g_col2 = st.columns([4, 1])
                with g_col1:
                    st.markdown(f"### {g_fields[3]}")
                    st.markdown(f"📁 **Category:** `{g_fields[4]}` &nbsp;|&nbsp; 🏷️ **Skills:** {render_tags(g_fields[5])}", unsafe_allow_html=True)
                    st.write(f"👨‍💻 **Offered by:** **{g_fields[2]}**")
                with g_col2:
                    st.subheader(f"${float(g_fields[6]):,.2f}")
                    if st.button(f"💬 Chat with {g_fields[2]}", key=f"contact_gig_{g_fields[0]}"):
                        st.session_state.active_chat_peer = g_fields[2]
                        st.success(f"Connecting you to {g_fields[2]}... Click the **💬 Messages & Payments** tab above.")
    else:
        st.info("No gigs have been posted yet.")
        
    if st.session_state.auth_role == "Freelancer":
        st.divider()
        st.subheader("⚡ Post a New Gig")
        with st.form("new_gig_form"):
            new_g_title = st.text_input("Gig description", placeholder="e.g., I will design an incredible mobile app")
            new_g_cat = st.selectbox("Category", ["Web Dev", "Mobile", "Data", "Design", "AI/ML"])
            new_g_tags = st.text_input("Skills required (space-separated)")
            # UI Form Constraint Floor exactly at $100
            new_g_price = st.number_input("Gig price ($)", min_value=100.0, value=100.0, step=10.0)
            if st.form_submit_button("Publish Gig ➔"):
                post_res = execute_core_command(f"add_gig,{st.session_state.auth_id},{new_g_title},{new_g_cat},{new_g_tags},{new_g_price}")
                if post_res[0] == "true":
                    st.success("Your gig is now live!")
                    st.rerun()
                else:
                    st.error("Could not post gig. Please check your details.")

# Module 3: Projects Briefs (Enforces $500 Floor Constraints)
with tab_projects:
    st.subheader("📢 Active Client Projects")
    execute_core_command("projects_list")
    proj_res = execute_core_command("projects_list")
    
    if proj_res[0] == "true" and int(proj_res[1]) > 0:
        projs_packed = proj_res[2].split('|') if len(proj_res) > 2 else []
        for proj_str in projs_packed:
            if not proj_str: continue
            p_fields = proj_str.split(',')
            with st.container(border=True):
                p_col1, p_col2 = st.columns([4, 1])
                with p_col1:
                    st.markdown(f"### {p_fields[3]}")
                    st.caption(f"📁 **Category:** `{p_fields[4]}` &nbsp;|&nbsp; 🏢 **Client:** **{p_fields[2]}**")
                    if len(p_fields) > 8 and p_fields[8] != "none":
                        st.markdown(f"📎 **Attached File:** `{p_fields[8]}`")
                with p_col2:
                    st.subheader(f"Budget: ${float(p_fields[5]):,.2f}")
                    if st.button(f"💬 Message {p_fields[2]}", key=f"chat_proj_{p_fields[0]}"):
                        st.session_state.active_chat_peer = p_fields[2]
                        st.success(f"Connecting to {p_fields[2]}... Click the **💬 Messages & Payments** tab above.")
    else:
        st.info("No projects are open right now. Check back later!")
        
    if st.session_state.auth_role == "Client":
        st.divider()
        st.subheader("📝 Post a New Project")
        with st.form("new_proj_form"):
            new_p_title = st.text_input("Project title", placeholder="Describe exactly what features you need developed...")
            new_p_cat = st.selectbox("Category", ["Web Dev", "Mobile", "Data", "Design", "AI/ML"])
            # UI Form Constraint Floor exactly at $500
            new_p_budget = st.number_input("Budget ($)", min_value=500.0, value=500.0, step=50.0)
            uploaded_file = st.file_uploader("Upload Project Guidelines/Files (Optional)", type=["pdf", "docx", "txt", "zip"])
            
            if st.form_submit_button("Publish Project ➔"):
                attachment_path = "none"
                if uploaded_file is not None:
                    upload_dir = "uploads"
                    os.makedirs(upload_dir, exist_ok=True)
                    safe_filename = f"file_{st.session_state.auth_id}_{uploaded_file.name}"
                    file_dest = os.path.join(upload_dir, safe_filename)
                    with open(file_dest, "wb") as f:
                        f.write(uploaded_file.getbuffer())
                    attachment_path = file_dest
                    
                post_p = execute_core_command(f"add_project,{st.session_state.auth_id},{new_p_title},{new_p_cat},{new_p_budget},{attachment_path}")
                if post_p[0] == "true":
                    st.success("Project posted successfully!")
                    st.rerun()
                else:
                    st.error("Could not post project. Make sure you have enough funds.")

# Module 4: Chat & Dynamic Payments (Enforces floors, completely removes caps, prevents self-chat)
with tab_chat:
    if not st.session_state.auth_id:
        st.warning("⚠️ **Please sign in:** You need an account to send private messages and transfer payments.")
    else:
        st.subheader("💬 Private Chat")
        users_res = execute_core_command("users_list")
        
        if users_res[0] == "true":
            all_registered_handles = users_res[1:]
            
            # --- Self-Chat Prevention Filter ---
            peer_options = [u for u in all_registered_handles if u != st.session_state.auth_name]
            
            default_index = 0
            if st.session_state.active_chat_peer in peer_options:
                default_index = peer_options.index(st.session_state.active_chat_peer)
                
            selected_peer = st.selectbox("Choose someone to talk to", peer_options, index=default_index)
            
            if selected_peer:
                st.session_state.active_chat_peer = selected_peer
                execute_core_command(f"get_msgs,{st.session_state.auth_name},{selected_peer}") 
                msgs_res = execute_core_command(f"get_msgs,{st.session_state.auth_name},{selected_peer}")
                
                with st.container(height=320, border=True):
                    if msgs_res[0] == "true" and msgs_res[1] != "none":
                        history_lines = msgs_res[1].split('|')
                        for item in history_lines:
                            s_node, s_text = item.split(':', 1)
                            if s_node == st.session_state.auth_name:
                                st.markdown(f"<div class='chat-bubble-you'><b>[You]:</b> {s_text}</div>", unsafe_allow_html=True)
                            else:
                                st.markdown(f"<div class='chat-bubble-peer'><b>[{s_node}]:</b> {s_text}</div>", unsafe_allow_html=True)
                    else:
                        st.caption("📭 No messages yet. Start the conversation below!")
                        
                with st.form("send_msg_form", clear_on_submit=True):
                    outbound_buffer = st.text_input("Type your message...")
                    if st.form_submit_button("Send Message ➔"):
                        if outbound_buffer:
                            execute_core_command(f"send_msg,{st.session_state.auth_name},{selected_peer},{outbound_buffer}")
                            st.rerun()
                            
                # Secure Dynamic Payments
                st.divider()
                st.subheader("💳 Secure Payments")
                
                peer_id_resolved = 0
                for i in range(1, 100):
                    p_test = execute_core_command(f"profile,{i}")
                    if p_test[0] == "true" and p_test[2] == selected_peer:
                        peer_id_resolved = i
                        break
                        
                if peer_id_resolved > 0:
                    if st.session_state.auth_role == "Client":
                        asking_price = 0.0
                        g_lookup = execute_core_command("gigs_list")
                        if g_lookup[0] == "true" and int(g_lookup[1]) > 0:
                            g_items = g_lookup[2].split('|') if len(g_lookup) > 2 else []
                            for g in g_items:
                                if not g: continue
                                gf = g.split(',')
                                if gf[2] == selected_peer:
                                    asking_price = float(gf[6])
                                    break
                                    
                        if asking_price > 0:
                            st.info(f"💡 The active asking price established by **{selected_peer}** is **${asking_price:,.2f}**")
                            
                        # Sets the input floor dynamically to match the milestone target while preventing underpayment below $100
                        floor_limit = max(100.0, asking_price)
                        pay_amt = st.number_input("Amount to Pay ($)", min_value=5.0, value=floor_limit, step=10.0)
                        
                        if st.button(f"Pay {selected_peer} Now ➔", use_container_width=True):
                            pay_res = execute_core_command(f"pay,{st.session_state.auth_id},{peer_id_resolved},{pay_amt},{asking_price}")
                            if pay_res[0] == "true":
                                st.success("Payment sent successfully!")
                                execute_core_command(f"send_msg,{st.session_state.auth_name},{selected_peer},Payment of ${pay_amt} sent successfully.")
                                st.rerun()
                            else:
                                # Captures exact "Insufficient balance" overdraft/underpayment errors directly from the C++ guards
                                failure_reason = pay_res[1] if len(pay_res) > 1 else "Payment failed: Transaction declined."
                                st.error(failure_reason)
                    else:
                        if st.button(f"Ask {selected_peer} for Payment ➔", use_container_width=True):
                            execute_core_command(f"send_msg,{st.session_state.auth_name},{selected_peer},Please release payment for the completed work.")
                            st.success("Payment request sent to client!")
                            st.rerun()
        else:
            st.error("Could not load user directory.")

# Module 5: Quality Reviews (Replaced notifications)
with tab_reviews:
    st.subheader("⭐ Platform Reviews")
    st.caption("Leave permanent performance feedback for completed milestones.")
    
    if not st.session_state.auth_id:
        st.warning("⚠️ Please sign in to submit evaluations.")
    else:
        with st.form("eval_form"):
            eval_target = st.number_input("User ID to Rate", min_value=1, step=1)
            eval_score = st.slider("Stars", 1, 5, 5)
            eval_comment = st.text_area("Review Comment", placeholder="Write about your experience working together...")
            
            if st.form_submit_button("Submit Review ➔"):
                if eval_comment:
                    rev_res = execute_core_command(f"review,{st.session_state.auth_id},{eval_target},{eval_score},{eval_comment}")
                    if rev_res[0] == "true":
                        st.success(f"Review posted! Their new average rating is: **{rev_res[1]}** ⭐")
                    else:
                        err_msg = rev_res[1] if len(rev_res) > 1 else "Could not post review."
                        st.error(f"Error: {err_msg}")
